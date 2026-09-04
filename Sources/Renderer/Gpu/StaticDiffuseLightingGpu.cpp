#include "Renderer/Gpu/StaticDiffuseLightingGpu.hpp"

#include "Renderer/Gpu/GBufferGpu.hpp"
#include "Renderer/Gpu/Gpu.hpp"
#include "Renderer/Gpu/ResourceLifecycle.hpp"
#include "Renderer/Gpu/StaticDirectSplitPolicy.hpp"
#include "Renderer/Gpu/StaticShadowCacheGpu.hpp"

namespace Renderer
{
namespace Gpu
{
namespace
{

constexpr const char *STATIC_DIFFUSE_COMPUTE=R"GLSL(
#version 430 core
layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
layout(binding=0) uniform sampler2D sPositionDepth;
layout(binding=1) uniform sampler2D sNormalRoughness;
layout(binding=2) uniform sampler2D sAlbedoMetallic;
layout(binding=3) uniform sampler2D sEmissive;
layout(binding=4) uniform sampler2D sAmbientTransmission;
layout(binding=6) uniform sampler2D sStaticShadow;
layout(rgba16f,binding=0) writeonly uniform image2D oStaticDiffuse;
struct LightData{vec4 positionType;vec4 directionRange;vec4 colorIntensity;vec4 coneShadow;};
layout(std430,binding=8) readonly buffer LightBuffer{LightData lights[];};
uniform int uStaticLightIndex;
uniform int uStaticShadowEnabled;
uniform mat4 uStaticShadowViewProjection;
const float PI=3.14159265358979323846;
const float EPSILON=0.00001;
float staticShadowVisibility(vec3 position,vec3 normal,vec3 lightDirection){vec4 clip=uStaticShadowViewProjection*vec4(position,1.0);if(abs(clip.w)<=EPSILON)return 1.0;vec3 ndc=clip.xyz/clip.w;vec2 uv=ndc.xy*0.5+0.5;float receiverDepth=ndc.z*0.5+0.5;if(uv.x<=0.0||uv.x>=1.0||uv.y<=0.0||uv.y>=1.0||receiverDepth<=0.0||receiverDepth>=1.0)return 1.0;vec2 texel=1.0/vec2(textureSize(sStaticShadow,0));float slope=1.0-max(dot(normalize(normal),normalize(lightDirection)),0.0);float bias=0.00035+slope*0.00125;float visible=0.0;for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){float blocker=texture(sStaticShadow,uv+vec2(x,y)*texel).r;visible+=receiverDepth-bias<=blocker?1.0:0.0;}return visible/9.0;}
void main(){ivec2 pixel=ivec2(gl_GlobalInvocationID.xy),dimensions=imageSize(oStaticDiffuse);if(pixel.x>=dimensions.x||pixel.y>=dimensions.y)return;vec4 pd=texelFetch(sPositionDepth,pixel,0);if(pd.w<=0.0){imageStore(oStaticDiffuse,pixel,vec4(0.055,0.070,0.105,1.0));return;}vec4 nr=texelFetch(sNormalRoughness,pixel,0),am=texelFetch(sAlbedoMetallic,pixel,0),eo=texelFetch(sEmissive,pixel,0),ambientTransmission=texelFetch(sAmbientTransmission,pixel,0);vec3 position=pd.xyz,normal=normalize(nr.xyz),albedo=am.xyz;float metallic=clamp(am.w,0.0,1.0);vec3 result=eo.xyz+ambientTransmission.rgb*albedo*0.025;if(uStaticLightIndex>=0){LightData light=lights[uStaticLightIndex];if(int(light.positionType.w+0.5)==0){vec3 ld=normalize(-light.directionRange.xyz);float nl=max(dot(normal,ld),0.0);if(nl>0.0){float visibility=(uStaticShadowEnabled!=0&&light.coneShadow.z>0.5)?staticShadowVisibility(position,normal,ld):1.0;vec3 radiance=light.colorIntensity.xyz*light.colorIntensity.w;vec3 lambert=albedo*(1.0-metallic)/PI;result+=lambert*radiance*nl*visibility;}}}imageStore(oStaticDiffuse,pixel,vec4(result,1.0));}
)GLSL";

void setError(std::string *error,const char *message)
{
    if(error)*error=message?message:"GPU static diffuse error";
}

bool ensureTexture(GLuint *texture,int width,int height)
{
    if(!texture)return false;
    if(*texture==0)*texture=lwcgl_glGenTexture();
    if(*texture==0)return false;
    glBindTexture(GL_TEXTURE_2D,*texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,width,height,0,GL_RGBA,GL_FLOAT,nullptr);
    return true;
}

void bindTexture(GLuint unit,GLuint texture)
{
    GLModern.glActiveTexture(static_cast<GLenum>(GL_TEXTURE0+unit));
    glBindTexture(GL_TEXTURE_2D,texture);
}

} // namespace

bool StaticDiffuseLightingGpu::init(std::string *error)
{
    shutdown();
    program_=createComputeProgram(STATIC_DIFFUSE_COMPUTE,error);
    if(program_==0)return false;
    static_light_index_location_=GL20.glGetUniformLocation(program_,"uStaticLightIndex");
    shadow_enabled_location_=GL20.glGetUniformLocation(program_,"uStaticShadowEnabled");
    shadow_matrix_location_=GL20.glGetUniformLocation(program_,"uStaticShadowViewProjection");
    if(static_light_index_location_<0||shadow_enabled_location_<0||shadow_matrix_location_<0)
    {
        setError(error,"GPU static diffuse uniforms are unavailable");shutdown();return false;
    }
    if(error)error->clear();return true;
}

bool StaticDiffuseLightingGpu::resize(int width,int height,std::string *error)
{
    const int target_width=normalizedExtent(width),target_height=normalizedExtent(height);
    if(!resizeStorageRequired(width_,height_,texture_!=0,width,height))return true;
    width_=target_width;height_=target_height;
    if(!ensureTexture(&texture_,width_,height_)){setError(error,"failed to allocate static diffuse texture");return false;}
    glBindTexture(GL_TEXTURE_2D,0);invalidate();if(error)error->clear();return true;
}

void StaticDiffuseLightingGpu::setLightSource(GLuint light_buffer,int static_light_index)
{
    if(light_buffer_!=light_buffer||static_light_index_!=static_light_index)invalidate();
    light_buffer_=light_buffer;static_light_index_=static_light_index;
}

bool StaticDiffuseLightingGpu::validFor(const RevisionState& revisions) const
{
    return valid_&&staticDiffuseValid(revisions_,revisions);
}

bool StaticDiffuseLightingGpu::updateIfNeeded(
            const GBufferGpu& gbuffer,
            const StaticShadowCacheGpu& shadow,
            const RevisionState& revisions,
            std::string *error)
{
    if(validFor(revisions)){if(error)error->clear();return true;}
    if(!ready()||!gbuffer.ready()||gbuffer.width()!=width_||gbuffer.height()!=height_
            ||light_buffer_==0)
    {setError(error,"GPU static diffuse resources are not ready");return false;}

    bindTexture(0,gbuffer.positionDepthTexture());bindTexture(1,gbuffer.normalRoughnessTexture());
    bindTexture(2,gbuffer.albedoMetallicTexture());bindTexture(3,gbuffer.emissiveTexture());
    bindTexture(4,gbuffer.transmissionTexture());bindTexture(6,shadow.enabled()?shadow.depthTexture():0);
    GL20.glUseProgram(program_);
    GL20.glUniform1i(static_light_index_location_,static_light_index_);
    GL20.glUniform1i(shadow_enabled_location_,shadow.enabled()&&static_light_index_>=0?1:0);
    GL20.glUniformMatrix4fv(shadow_matrix_location_,1,GL_FALSE,shadow.lightViewProjection().value);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,8,light_buffer_);
    GL42.glBindImageTexture(0,texture_,0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA16F);
    GL43.glDispatchCompute(static_cast<GLuint>((width_+7)/8),static_cast<GLuint>((height_+7)/8),1);
    GL42.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
    GL20.glUseProgram(0);GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,8,0);
    for(GLuint unit=0;unit<=6;++unit)bindTexture(unit,0);GLModern.glActiveTexture(GL_TEXTURE0);
    revisions_=revisions;valid_=true;if(error)error->clear();return true;
}

void StaticDiffuseLightingGpu::invalidate(){valid_=false;}

void StaticDiffuseLightingGpu::shutdown()
{
    if(texture_!=0)glDeleteTextures(texture_);texture_=0;destroyProgram(&program_);
    light_buffer_=0;static_light_index_location_=shadow_enabled_location_=shadow_matrix_location_=-1;
    revisions_={};static_light_index_=-1;width_=0;height_=0;valid_=false;
}

} // namespace Gpu
} // namespace Renderer
