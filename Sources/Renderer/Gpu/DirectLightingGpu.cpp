#include "Renderer/Gpu/DirectLightingGpu.hpp"
#include "Renderer/Gpu/DirectLightingImportedShader.hpp"
#include "Renderer/Gpu/Gpu.hpp"
#include "Renderer/Gpu/ResourceLifecycle.hpp"
#include "Renderer/Gpu/SurfaceFormats.hpp"
#include "Renderer/Gpu/TransparentGpu.hpp"

#include <algorithm>
#include <exception>
#include <memory>
#include <string>

namespace Renderer { namespace Gpu { namespace {

constexpr const char *DIRECT_COMBINE_COMPUTE=R"GLSL(
#version 430 core
layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
layout(binding=0) uniform sampler2D sStaticDiffuse;
layout(binding=1) uniform sampler2D sViewSpecular;
layout(binding=2) uniform sampler2D sDynamicDirect;
layout(rgba16f,binding=0) writeonly uniform image2D oDirect;
void main(){ivec2 pixel=ivec2(gl_GlobalInvocationID.xy),dimensions=imageSize(oDirect);if(pixel.x>=dimensions.x||pixel.y>=dimensions.y)return;vec3 value=texelFetch(sStaticDiffuse,pixel,0).xyz+texelFetch(sViewSpecular,pixel,0).xyz+texelFetch(sDynamicDirect,pixel,0).xyz;imageStore(oDirect,pixel,vec4(max(value,vec3(0.0)),1.0));}
)GLSL";

GLint surfaceInternalFormat(SurfaceFormat format){return format==SurfaceFormat::Rgba8?GL_RGBA8:GL_RGBA16F;}
GLenum surfacePixelType(SurfaceFormat format){return format==SurfaceFormat::Rgba8?GL_UNSIGNED_BYTE:GL_FLOAT;}
bool ensureTexture(GLuint *texture,int width,int height,SurfaceFormat format){if(!texture)return false;if(*texture==0)*texture=lwcgl_glGenTexture();if(*texture==0)return false;glBindTexture(GL_TEXTURE_2D,*texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexImage2D(GL_TEXTURE_2D,0,surfaceInternalFormat(format),width,height,0,GL_RGBA,surfacePixelType(format),nullptr);return true;}
void deleteTexture(GLuint *texture){if(!texture||*texture==0)return;glDeleteTextures(*texture);*texture=0;}
void setError(std::string *error,const char *message){if(error)*error=message?message:"GPU direct lighting error";}

} // namespace

DirectLightingGpu::~DirectLightingGpu(){shutdown();}

bool DirectLightingGpu::init(std::string *error)
{
    shutdown();
    try
    {
        const std::string shader=directLightingImportedShader();
        program_=createComputeProgram(shader.c_str(),error);
    }
    catch(const std::exception& exception)
    {
        if(error)*error=exception.what();
        return false;
    }
    if(program_==0)return false;
    combine_program_=createComputeProgram(DIRECT_COMBINE_COMPUTE,error);
    if(combine_program_==0){shutdown();return false;}
    camera_location_=GL20.glGetUniformLocation(program_,"uCameraPosition");
    light_count_location_=GL20.glGetUniformLocation(program_,"uLightCount");
    primitive_count_location_=GL20.glGetUniformLocation(program_,"uPrimitiveCount");
    static_split_light_index_location_=GL20.glGetUniformLocation(program_,"uStaticSplitLightIndex");
    inverse_view_projection_location_=GL20.glGetUniformLocation(program_,"uGBufferInverseViewProjection");
    if(camera_location_<0||light_count_location_<0||primitive_count_location_<0
            ||static_split_light_index_location_<0||inverse_view_projection_location_<0)
    {
        setError(error,"GPU direct lighting uniforms are unavailable");shutdown();return false;
    }
    GL15.glGenBuffers(1,&light_buffer_);GL15.glGenBuffers(1,&primitive_buffer_);
    if(light_buffer_==0||primitive_buffer_==0){setError(error,"failed to allocate GPU lighting scene buffers");shutdown();return false;}
    if(!virtual_shadow_map_.init(error)
            ||!smrt_shadow_.init(error)
            ||!static_diffuse_.init(error)
            ||!view_specular_.init(error))
    {
        shutdown();return false;
    }
    transparent_=std::make_unique<TransparentGpu>();
    if(!transparent_->init(error)){shutdown();return false;}
    if(error)error->clear();
    return true;
}

bool DirectLightingGpu::resize(int width,int height,std::string *error)
{
    const int target_width=normalizedExtent(width),target_height=normalizedExtent(height);
    const bool resized=resizeStorageRequired(width_,height_,direct_color_!=0&&dynamic_color_!=0,width,height);
    if(resized)
    {
        width_=target_width;height_=target_height;
        if(!ensureTexture(&direct_color_,width_,height_,DIRECT_COLOR_FORMAT)
                ||!ensureTexture(&dynamic_color_,width_,height_,DIRECT_COLOR_FORMAT))
        {
            setError(error,"failed to allocate GPU direct lighting textures");return false;
        }
    }
    if(!virtual_shadow_map_.resize(target_width,target_height,error)
            ||!smrt_shadow_.resize(target_width,target_height,error)
            ||!static_diffuse_.resize(target_width,target_height,error)
            ||!view_specular_.resize(target_width,target_height,error))
        return false;
    if(transparent_&&!transparent_->resize(target_width,target_height,error))return false;
    glBindTexture(GL_TEXTURE_2D,0);if(error)error->clear();return true;
}

bool DirectLightingGpu::uploadBuffer(GLuint buffer,std::size_t *capacity,const void *data,std::size_t size,std::string *error)
{
    if(buffer==0||!capacity){setError(error,"invalid GPU lighting upload destination");return false;}
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER,buffer);
    if(size==0){if(*capacity==0){GL15.glBufferData(GL_SHADER_STORAGE_BUFFER,16,nullptr,GL_DYNAMIC_DRAW);*capacity=16;}return true;}
    if(size>*capacity){std::size_t new_capacity=std::max<std::size_t>(256u,*capacity);while(new_capacity<size)new_capacity*=2u;GL15.glBufferData(GL_SHADER_STORAGE_BUFFER,static_cast<LWCGLsizeiptr>(new_capacity),nullptr,GL_DYNAMIC_DRAW);*capacity=new_capacity;}
    GL15.glBufferSubData(GL_SHADER_STORAGE_BUFFER,0,static_cast<LWCGLsizeiptr>(size),data);return true;
}

bool DirectLightingGpu::render(const Ecs::World& world,const GBufferGpu& gbuffer,const Math::Vec3& camera_position,std::uint64_t frame_index,std::string *error){return updateScene(world,error)&&dispatch(gbuffer,camera_position,frame_index,error);}
void DirectLightingGpu::destroyTextures(){deleteTexture(&direct_color_);deleteTexture(&dynamic_color_);}
void DirectLightingGpu::releaseAcceleration(){if(bvh_node_buffer_!=0)GL15.glDeleteBuffers(1,&bvh_node_buffer_);bvh_node_buffer_=0;bvh_node_capacity_=0;bvh_primitive_count_=0;bvh_nodes_.clear();uploaded_bvh_nodes_.clear();use_bvh_=false;}
void DirectLightingGpu::shutdown(){if(transparent_){transparent_->shutdown();transparent_.reset();}view_specular_.shutdown();static_diffuse_.shutdown();smrt_shadow_.shutdown();virtual_shadow_map_.shutdown();trace_geometry_.shutdown();triangle_scene_.shutdown();scene_world_=nullptr;scene_revision_=0u;releaseAcceleration();destroyTextures();if(light_buffer_!=0){GL15.glDeleteBuffers(1,&light_buffer_);light_buffer_=0;}if(primitive_buffer_!=0){GL15.glDeleteBuffers(1,&primitive_buffer_);primitive_buffer_=0;}destroyProgram(&program_);destroyProgram(&combine_program_);light_capacity_=0;primitive_capacity_=0;lights_.clear();primitives_.clear();uploaded_lights_.clear();uploaded_primitives_.clear();primitive_bounds_.clear();camera_location_=-1;light_count_location_=-1;primitive_count_location_=-1;static_split_light_index_location_=-1;inverse_view_projection_location_=-1;bench_config_={};bench_config_initialized_=false;bench_reported_=false;use_bvh_=false;width_=0;height_=0;}

} }
