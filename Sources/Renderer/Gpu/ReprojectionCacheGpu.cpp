#include "Renderer/Gpu/ReprojectionCacheGpu.hpp"

#include "Renderer/Gpu/GBufferGpu.hpp"
#include "Renderer/Gpu/Gpu.hpp"
#include "Renderer/Gpu/ReprojectionShader.hpp"
#include "Renderer/Gpu/ResourceLifecycle.hpp"

namespace Renderer
{
namespace Gpu
{
namespace
{

void setError(std::string *error,const char *message)
{
    if(error) *error=message?message:"GPU reprojection cache error";
}

bool ensureFloatTexture(GLuint *texture,int width,int height)
{
    if(!texture) return false;
    if(*texture==0) *texture=lwcgl_glGenTexture();
    if(*texture==0) return false;
    glBindTexture(GL_TEXTURE_2D,*texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,width,height,0,GL_RGBA,GL_FLOAT,nullptr);
    return true;
}

bool ensureUintTexture(GLuint *texture,int width,int height,GLint internal_format,GLenum type)
{
    if(!texture) return false;
    if(*texture==0) *texture=lwcgl_glGenTexture();
    if(*texture==0) return false;
    glBindTexture(GL_TEXTURE_2D,*texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,internal_format,width,height,0,GL_RED_INTEGER,type,nullptr);
    return true;
}

void deleteTexture(GLuint *texture)
{
    if(!texture||*texture==0) return;
    glDeleteTextures(*texture);
    *texture=0;
}

void bindTextureUnit(GLuint unit,GLuint texture)
{
    GLModern.glActiveTexture(static_cast<GLenum>(GL_TEXTURE0+unit));
    glBindTexture(GL_TEXTURE_2D,texture);
}

void unbindTextureUnits(GLuint count)
{
    for(GLuint unit=0;unit<count;++unit) bindTextureUnit(unit,0);
    GLModern.glActiveTexture(GL_TEXTURE0);
}

} // namespace

bool ReprojectionCacheGpu::init(std::string *error)
{
    shutdown();
    reprojection_program_=createComputeProgram(REPROJECTION_COMPUTE,error);
    if(reprojection_program_==0) return false;
    capture_program_=createComputeProgram(REPROJECTION_CAPTURE_COMPUTE,error);
    if(capture_program_==0){shutdown();return false;}

    previous_view_projection_location_=GL20.glGetUniformLocation(
        reprojection_program_,"uPreviousViewProjection");
    camera_position_location_=GL20.glGetUniformLocation(
        reprojection_program_,"uCameraPosition");
    history_valid_location_=GL20.glGetUniformLocation(
        reprojection_program_,"uHistoryValid");
    if(previous_view_projection_location_<0||camera_position_location_<0
            ||history_valid_location_<0)
    {
        setError(error,"GPU reprojection uniforms are unavailable");
        shutdown();
        return false;
    }
    if(error) error->clear();
    return true;
}

bool ReprojectionCacheGpu::ensure(int width,int height,std::string *error)
{
    if(reprojection_program_==0||capture_program_==0)
    {
        if(!init(error)) return false;
    }
    return resize(width,height,error);
}

bool ReprojectionCacheGpu::resize(int width,int height,std::string *error)
{
    const bool resources_ready=previous_position_!=0&&previous_normal_!=0
        &&previous_material_!=0&&reprojected_indirect_!=0
        &&reprojected_reflection_!=0&&valid_mask_!=0;
    if(!resizeStorageRequired(width_,height_,resources_ready,width,height))
        return true;

    width_=normalizedExtent(width);
    height_=normalizedExtent(height);
    trace_width_=(width_+1)/2;
    trace_height_=(height_+1)/2;

    const bool ok=ensureFloatTexture(&previous_position_,trace_width_,trace_height_)
        &&ensureFloatTexture(&previous_normal_,trace_width_,trace_height_)
        &&ensureUintTexture(&previous_material_,trace_width_,trace_height_,GL_R32UI,GL_UNSIGNED_INT)
        &&ensureFloatTexture(&reprojected_indirect_,trace_width_,trace_height_)
        &&ensureFloatTexture(&reprojected_reflection_,trace_width_,trace_height_)
        &&ensureUintTexture(&valid_mask_,trace_width_,trace_height_,GL_R8UI,GL_UNSIGNED_BYTE);
    glBindTexture(GL_TEXTURE_2D,0);
    if(!ok)
    {
        setError(error,"failed to allocate GPU reprojection textures");
        destroyTextures();
        return false;
    }
    invalidate();
    if(error) error->clear();
    return true;
}

bool ReprojectionCacheGpu::reproject(
            const GBufferGpu& gbuffer,
            GLuint previous_indirect,
            GLuint previous_reflection,
            const Math::Vec3& camera_position,
            std::string *error)
{
    if(!ready()||!gbuffer.ready()||gbuffer.width()!=width_||gbuffer.height()!=height_
            ||previous_indirect==0||previous_reflection==0)
    {
        setError(error,"GPU reprojection resources are not ready");
        return false;
    }

    bindTextureUnit(0,gbuffer.positionDepthTexture());
    bindTextureUnit(1,gbuffer.normalRoughnessTexture());
    bindTextureUnit(2,gbuffer.materialIdentityTexture());
    bindTextureUnit(3,previous_position_);
    bindTextureUnit(4,previous_normal_);
    bindTextureUnit(5,previous_material_);
    bindTextureUnit(6,previous_indirect);
    bindTextureUnit(7,previous_reflection);

    GL20.glUseProgram(reprojection_program_);
    GL20.glUniformMatrix4fv(previous_view_projection_location_,1,GL_FALSE,
                            previous_view_projection_.value);
    GL20.glUniform3f(camera_position_location_,camera_position.x,
                     camera_position.y,camera_position.z);
    GL20.glUniform1i(history_valid_location_,history_valid_?1:0);
    GL42.glBindImageTexture(0,reprojected_indirect_,0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA16F);
    GL42.glBindImageTexture(1,reprojected_reflection_,0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA16F);
    GL42.glBindImageTexture(2,valid_mask_,0,GL_FALSE,0,GL_WRITE_ONLY,GL_R8UI);
    GL43.glDispatchCompute(static_cast<GLuint>((trace_width_+7)/8),
                           static_cast<GLuint>((trace_height_+7)/8),1);
    GL42.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);

    GL20.glUseProgram(0);
    unbindTextureUnits(8);
    if(error) error->clear();
    return true;
}

bool ReprojectionCacheGpu::capture(
            const GBufferGpu& gbuffer,
            const Math::Mat4& view_projection,
            std::string *error)
{
    if(!ready()||!gbuffer.ready()||gbuffer.width()!=width_||gbuffer.height()!=height_)
    {
        setError(error,"GPU reprojection capture resources are not ready");
        return false;
    }

    bindTextureUnit(0,gbuffer.positionDepthTexture());
    bindTextureUnit(1,gbuffer.normalRoughnessTexture());
    bindTextureUnit(2,gbuffer.materialIdentityTexture());
    GL20.glUseProgram(capture_program_);
    GL42.glBindImageTexture(0,previous_position_,0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA16F);
    GL42.glBindImageTexture(1,previous_normal_,0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA16F);
    GL42.glBindImageTexture(2,previous_material_,0,GL_FALSE,0,GL_WRITE_ONLY,GL_R32UI);
    GL43.glDispatchCompute(static_cast<GLuint>((trace_width_+7)/8),
                           static_cast<GLuint>((trace_height_+7)/8),1);
    GL42.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);

    GL20.glUseProgram(0);
    unbindTextureUnits(3);
    previous_view_projection_=view_projection;
    history_valid_=true;
    if(error) error->clear();
    return true;
}

void ReprojectionCacheGpu::invalidate()
{
    history_valid_=false;
}

void ReprojectionCacheGpu::destroyTextures()
{
    deleteTexture(&previous_position_);
    deleteTexture(&previous_normal_);
    deleteTexture(&previous_material_);
    deleteTexture(&reprojected_indirect_);
    deleteTexture(&reprojected_reflection_);
    deleteTexture(&valid_mask_);
    width_=0;height_=0;trace_width_=0;trace_height_=0;
    history_valid_=false;
    previous_view_projection_=Math::identity();
}

void ReprojectionCacheGpu::shutdown()
{
    destroyTextures();
    destroyProgram(&reprojection_program_);
    destroyProgram(&capture_program_);
    previous_view_projection_location_=-1;
    camera_position_location_=-1;
    history_valid_location_=-1;
}

bool ReprojectionCacheGpu::ready() const
{
    return reprojection_program_!=0&&capture_program_!=0
        &&previous_position_!=0&&previous_normal_!=0&&previous_material_!=0
        &&reprojected_indirect_!=0&&reprojected_reflection_!=0&&valid_mask_!=0;
}

} // namespace Gpu
} // namespace Renderer
