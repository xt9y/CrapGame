#include "Renderer/Gpu/ViewSpecularGpu.hpp"

#include "Renderer/Gpu/DirtyTileGpu.hpp"
#include "Renderer/Gpu/GBufferGpu.hpp"
#include "Renderer/Gpu/Gpu.hpp"
#include "Renderer/Gpu/ResourceLifecycle.hpp"
#include "Renderer/Gpu/StaticShadowCacheGpu.hpp"

namespace Renderer
{
namespace Gpu
{
namespace
{

constexpr const char *VIEW_SPECULAR_COMPUTE=R"GLSL(
#version 430 core
layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
layout(binding=0) uniform sampler2D sPositionDepth;
layout(binding=1) uniform sampler2D sNormalRoughness;
layout(binding=2) uniform sampler2D sAlbedoMetallic;
layout(binding=3) uniform sampler2D sSpecularIor;
layout(binding=4) uniform sampler2D sAdvancedMaterial;
layout(binding=5) uniform sampler2D sTangentAnisotropy;
layout(binding=6) uniform sampler2D sStaticShadow;
layout(rgba16f,binding=0) writeonly uniform image2D oViewSpecular;
struct LightData{vec4 positionType;vec4 directionRange;vec4 colorIntensity;vec4 coneShadow;};
layout(std430,binding=8) readonly buffer LightBuffer{LightData lights[];};
uniform vec3 uCameraPosition;
uniform int uStaticLightIndex;
uniform int uStaticShadowEnabled;
uniform mat4 uStaticShadowViewProjection;
const float PI=3.14159265358979323846;
const float EPSILON=0.00001;
float staticShadowVisibility(vec3 position,vec3 normal,vec3 lightDirection){vec4 clip=uStaticShadowViewProjection*vec4(position,1.0);if(abs(clip.w)<=EPSILON)return 1.0;vec3 ndc=clip.xyz/clip.w;vec2 uv=ndc.xy*0.5+0.5;float receiverDepth=ndc.z*0.5+0.5;if(uv.x<=0.0||uv.x>=1.0||uv.y<=0.0||uv.y>=1.0||receiverDepth<=0.0||receiverDepth>=1.0)return 1.0;vec2 texel=1.0/vec2(textureSize(sStaticShadow,0));float slope=1.0-max(dot(normalize(normal),normalize(lightDirection)),0.0);float bias=0.00035+slope*0.00125;float visible=0.0;for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){float blocker=texture(sStaticShadow,uv+vec2(x,y)*texel).r;visible+=receiverDepth-bias<=blocker?1.0:0.0;}return visible/9.0;}
float geometrySchlickGgx(float c,float r){float x=r+1.0,k=x*x/8.0;return c/(c*(1.0-k)+k+EPSILON);}
float geometrySmith(vec3 n,vec3 v,vec3 l,float r){return geometrySchlickGgx(clamp(dot(n,v),0.0,1.0),r)*geometrySchlickGgx(clamp(dot(n,l),0.0,1.0),r);}
vec3 fresnelSchlick(float c,vec3 f0){float f=pow(1.0-clamp(c,0.0,1.0),5.0);return f0+(vec3(1.0)-f0)*f;}
float distributionGgx(vec3 n,vec3 h,float r){float a=r*r,a2=a*a,c=clamp(dot(n,h),0.0,1.0),c2=c*c,d=c2*(a2-1.0)+1.0;return a2/(PI*d*d+EPSILON);}
float distributionAnisotropic(vec3 n,vec3 t,vec3 b,vec3 h,float roughness,float anisotropy){float a=max(0.001,roughness*roughness);float aspect=sqrt(max(0.1,1.0-0.9*abs(anisotropy)));float at=anisotropy>=0.0?a/aspect:a*aspect;float ab=anisotropy>=0.0?a*aspect:a/aspect;float hx=dot(h,t),hy=dot(h,b),hz=max(EPSILON,dot(h,n));float d=hx*hx/(at*at)+hy*hy/(ab*ab)+hz*hz;return 1.0/(PI*at*ab*d*d+EPSILON);}
float iorToF0(float ior){float n=max(1.0001,ior),r=(n-1.0)/(n+1.0);return r*r;}
vec3 evaluateMaterialPbr(vec3 albedo,float metallic,float roughness,vec3 explicitSpecular,float ior,float clearcoat,float clearcoatRoughness,float sheen,float anisotropy,vec3 tangent,vec3 normal,vec3 viewDirection,vec3 lightDirection,vec3 radiance){vec3 n=normalize(normal),v=normalize(viewDirection),l=normalize(lightDirection),h=normalize(v+l);float nl=clamp(dot(n,l),0.0,1.0),nv=clamp(dot(n,v),0.0,1.0);if(nl<=0.0||nv<=0.0)return vec3(0);vec3 t=normalize(tangent-n*dot(n,tangent));vec3 b=normalize(cross(n,t));vec3 dielectricF0=dot(explicitSpecular,explicitSpecular)>EPSILON?clamp(explicitSpecular,vec3(0),vec3(1)):vec3(iorToF0(ior));vec3 f0=mix(dielectricF0,albedo,clamp(metallic,0.0,1.0));vec3 f=fresnelSchlick(dot(h,v),f0);float r=clamp(roughness,0.04,1.0);float d=abs(anisotropy)>0.001?distributionAnisotropic(n,t,b,h,r,anisotropy):distributionGgx(n,h,r);float g=geometrySmith(n,v,l,r);vec3 spec=f*d*g/(4.0*nv*nl+EPSILON);vec3 diffuseWeight=(vec3(1)-f)*(1.0-clamp(metallic,0.0,1.0));vec3 base=(diffuseWeight*(albedo/PI)+spec);float cc=clamp(clearcoat,0.0,1.0);if(cc>0.0){float cr=clamp(clearcoatRoughness,0.04,1.0);vec3 cf=fresnelSchlick(dot(h,v),vec3(0.04));float cd=distributionGgx(n,h,cr),cg=geometrySmith(n,v,l,cr);vec3 coat=cf*cd*cg/(4.0*nv*nl+EPSILON);base=base*(vec3(1)-cf*cc)+coat*cc;}float sheenFactor=clamp(sheen,0.0,1.0)*(1.0-clamp(metallic,0.0,1.0))*pow(1.0-clamp(dot(h,v),0.0,1.0),5.0);base+=mix(vec3(1),albedo,0.5)*sheenFactor;return base*radiance*nl;}
void main(){ivec2 pixel=ivec2(gl_GlobalInvocationID.xy),dimensions=imageSize(oViewSpecular);if(pixel.x>=dimensions.x||pixel.y>=dimensions.y)return;vec4 pd=texelFetch(sPositionDepth,pixel,0);if(pd.w<=0.0||uStaticLightIndex<0){imageStore(oViewSpecular,pixel,vec4(0.0));return;}vec4 nr=texelFetch(sNormalRoughness,pixel,0),am=texelFetch(sAlbedoMetallic,pixel,0),si=texelFetch(sSpecularIor,pixel,0),adv=texelFetch(sAdvancedMaterial,pixel,0),ta=texelFetch(sTangentAnisotropy,pixel,0);LightData light=lights[uStaticLightIndex];if(int(light.positionType.w+0.5)!=0){imageStore(oViewSpecular,pixel,vec4(0.0));return;}vec3 position=pd.xyz,normal=normalize(nr.xyz),albedo=am.xyz,ld=normalize(-light.directionRange.xyz),viewDirection=normalize(uCameraPosition-position);float nl=max(dot(normal,ld),0.0),metallic=clamp(am.w,0.0,1.0),roughness=nr.w;if(nl<=0.0){imageStore(oViewSpecular,pixel,vec4(0.0));return;}float visibility=(uStaticShadowEnabled!=0&&light.coneShadow.z>0.5)?staticShadowVisibility(position,normal,ld):1.0;vec3 radiance=light.colorIntensity.xyz*light.colorIntensity.w*visibility;vec3 full=evaluateMaterialPbr(albedo,metallic,roughness,si.rgb,si.a,adv.x,adv.y,adv.z,ta.a,ta.xyz,normal,viewDirection,ld,radiance);vec3 lambert=albedo*(1.0-metallic)/PI*radiance*nl;imageStore(oViewSpecular,pixel,vec4(full-lambert,1.0));}
)GLSL";

void setError(std::string *error,const char *message){if(error)*error=message?message:"GPU view specular error";}

bool ensureTexture(GLuint *texture,int width,int height)
{
    if(!texture)return false;if(*texture==0)*texture=lwcgl_glGenTexture();if(*texture==0)return false;
    glBindTexture(GL_TEXTURE_2D,*texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA16F,width,height,0,GL_RGBA,GL_FLOAT,nullptr);return true;
}
void bindTexture(GLuint unit,GLuint texture){GLModern.glActiveTexture(static_cast<GLenum>(GL_TEXTURE0+unit));glBindTexture(GL_TEXTURE_2D,texture);}

} // namespace

bool ViewSpecularGpu::init(std::string *error)
{
    shutdown();program_=createComputeProgram(VIEW_SPECULAR_COMPUTE,error);if(program_==0)return false;
    camera_location_=GL20.glGetUniformLocation(program_,"uCameraPosition");static_light_index_location_=GL20.glGetUniformLocation(program_,"uStaticLightIndex");shadow_enabled_location_=GL20.glGetUniformLocation(program_,"uStaticShadowEnabled");shadow_matrix_location_=GL20.glGetUniformLocation(program_,"uStaticShadowViewProjection");
    if(camera_location_<0||static_light_index_location_<0||shadow_enabled_location_<0||shadow_matrix_location_<0){setError(error,"GPU view specular uniforms are unavailable");shutdown();return false;}if(error)error->clear();return true;
}

bool ViewSpecularGpu::resize(int width,int height,std::string *error)
{
    const int target_width=normalizedExtent(width),target_height=normalizedExtent(height);if(!resizeStorageRequired(width_,height_,texture_!=0,width,height))return true;width_=target_width;height_=target_height;if(!ensureTexture(&texture_,width_,height_)){setError(error,"failed to allocate view specular texture");return false;}glBindTexture(GL_TEXTURE_2D,0);if(error)error->clear();return true;
}

void ViewSpecularGpu::setLightSource(GLuint light_buffer,int static_light_index){light_buffer_=light_buffer;static_light_index_=static_light_index;}

bool ViewSpecularGpu::render(const GBufferGpu& gbuffer,const StaticShadowCacheGpu& shadow,const Math::Vec3& camera,const DirtyTileGpu *dirty_tiles,std::string *error)
{
    (void)dirty_tiles;
    if(!ready()||!gbuffer.ready()||gbuffer.width()!=width_||gbuffer.height()!=height_||light_buffer_==0){setError(error,"GPU view specular resources are not ready");return false;}
    bindTexture(0,gbuffer.positionDepthTexture());bindTexture(1,gbuffer.normalRoughnessTexture());bindTexture(2,gbuffer.albedoMetallicTexture());bindTexture(3,gbuffer.specularIorTexture());bindTexture(4,gbuffer.advancedMaterialTexture());bindTexture(5,gbuffer.tangentAnisotropyTexture());bindTexture(6,shadow.enabled()?shadow.depthTexture():0);
    GL20.glUseProgram(program_);GL20.glUniform3f(camera_location_,camera.x,camera.y,camera.z);GL20.glUniform1i(static_light_index_location_,static_light_index_);GL20.glUniform1i(shadow_enabled_location_,shadow.enabled()&&static_light_index_>=0?1:0);GL20.glUniformMatrix4fv(shadow_matrix_location_,1,GL_FALSE,shadow.lightViewProjection().value);GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,8,light_buffer_);GL42.glBindImageTexture(0,texture_,0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA16F);GL43.glDispatchCompute(static_cast<GLuint>((width_+7)/8),static_cast<GLuint>((height_+7)/8),1);GL42.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);GL20.glUseProgram(0);GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,8,0);for(GLuint unit=0;unit<=6;++unit)bindTexture(unit,0);GLModern.glActiveTexture(GL_TEXTURE0);if(error)error->clear();return true;
}

void ViewSpecularGpu::shutdown()
{
    if(texture_!=0)glDeleteTextures(texture_);texture_=0;destroyProgram(&program_);light_buffer_=0;camera_location_=static_light_index_location_=shadow_enabled_location_=shadow_matrix_location_=-1;static_light_index_=-1;width_=0;height_=0;
}

} // namespace Gpu
} // namespace Renderer
