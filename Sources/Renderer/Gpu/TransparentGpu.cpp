#include "Renderer/Gpu/TransparentGpu.hpp"
#include "Renderer/Gpu/Gpu.hpp"
#include "Renderer/Gpu/ResourceLifecycle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace Renderer { namespace Gpu { namespace {

constexpr const char* VERTEX_SHADER=R"GLSL(
#version 430 core
layout(location=0)in vec3 aPosition;
layout(location=1)in vec3 aNormal;
layout(location=2)in vec2 aUv;
layout(location=3)in vec4 aTangent;
uniform mat4 uView,uProjection,uModel,uNormalMatrix;
out vec3 vWorldPosition,vNormal,vTangent;
out vec2 vUv;
out float vSign;
void main(){
    vec4 world=uModel*vec4(aPosition,1.0);
    vWorldPosition=world.xyz;
    vNormal=normalize(mat3(uNormalMatrix)*aNormal);
    vTangent=normalize(mat3(uNormalMatrix)*aTangent.xyz);
    vSign=aTangent.w;
    vUv=aUv;
    gl_Position=uProjection*uView*world;
}
)GLSL";

constexpr const char* FRAGMENT_SHADER=R"GLSL(
#version 430 core
in vec3 vWorldPosition,vNormal,vTangent;
in vec2 vUv;
in float vSign;
layout(location=0)out vec4 oColor;

layout(binding=0)uniform sampler2D sOpaque;
layout(binding=1)uniform sampler2D uBaseColorTex;
layout(binding=2)uniform sampler2D uAmbientTex;
layout(binding=3)uniform sampler2D uSpecularTex;
layout(binding=4)uniform sampler2D uEmissiveTex;
layout(binding=5)uniform sampler2D uMetallicTex;
layout(binding=6)uniform sampler2D uRoughnessTex;
layout(binding=7)uniform sampler2D uOpacityTex;
layout(binding=8)uniform sampler2D uNormalTex;
layout(binding=9)uniform sampler2D uBumpTex;
layout(binding=10)uniform sampler2D uReflectionTex;
layout(binding=11)uniform sampler2D uTransmissionTex;
layout(binding=12)uniform sampler2D uClearcoatTex;
layout(binding=13)uniform sampler2D uClearcoatRoughnessTex;
layout(binding=14)uniform sampler2D uSheenTex;
layout(binding=15)uniform sampler2D uAnisotropyTex;

struct LightData{vec4 positionType;vec4 directionRange;vec4 colorIntensity;vec4 coneShadow;};
layout(std430,binding=5)readonly buffer LightBuffer{LightData lights[];};

uniform vec3 uCameraPosition;
uniform vec2 uViewport;
uniform int uLightCount,uTextureMask;
uniform vec4 uScalar0,uScalar1,uScalar2,uColor0,uColor1;
uniform vec3 uAmbientColor,uTransmissionColor;
uniform vec4 uTexScaleOffset[15],uTexOptions[15];

const float PI=3.14159265358979323846;
const float EPS=0.00001;
const int SB=0,SA=1,SS=2,SE=3,SM=4,SR=5,SO=7,SN=8,SBUMP=9,SREF=11,STRANS=12,SCC=13,SCCR=14,SSH=15,SAN=16;

bool hasT(int slot){return(uTextureMask&(1<<slot))!=0;}
vec2 materialUv(int liveIndex){vec4 q=uTexScaleOffset[liveIndex];return vUv*q.xy+q.zw;}
float channel(vec4 value,int selected){if(selected==1)return value.g;if(selected==2)return value.b;if(selected==3)return value.a;if(selected==4)return dot(value.rgb,vec3(1.0/3.0));return value.r;}
float scalarSample(sampler2D textureValue,int liveIndex){return channel(texture(textureValue,materialUv(liveIndex)),int(uTexOptions[liveIndex].y+0.5));}
float iorToF0(float ior){float n=max(ior,1.0001),ratio=(n-1.0)/(n+1.0);return ratio*ratio;}
vec3 fresnelSchlick(float cosineValue,vec3 f0){float f=pow(1.0-clamp(cosineValue,0.0,1.0),5.0);return f0+(vec3(1.0)-f0)*f;}
float geometrySchlick(float cosineValue,float roughness){float r=roughness+1.0,k=r*r/8.0;return cosineValue/(cosineValue*(1.0-k)+k+EPS);}
float geometrySmith(vec3 n,vec3 v,vec3 l,float roughness){return geometrySchlick(max(dot(n,v),0.0),roughness)*geometrySchlick(max(dot(n,l),0.0),roughness);}
float distributionGgx(vec3 n,vec3 h,float roughness){float a=roughness*roughness,a2=a*a,c=max(dot(n,h),0.0),den=c*c*(a2-1.0)+1.0;return a2/(PI*den*den+EPS);}
float distributionAnisotropic(vec3 n,vec3 t,vec3 b,vec3 h,float roughness,float anisotropy){float a=max(0.001,roughness*roughness),aspect=sqrt(max(0.1,1.0-0.9*abs(anisotropy)));float at=anisotropy>=0.0?a/aspect:a*aspect,ab=anisotropy>=0.0?a*aspect:a/aspect;float hx=dot(h,t),hy=dot(h,b),hz=max(EPS,dot(h,n));float d=hx*hx/(at*at)+hy*hy/(ab*ab)+hz*hz;return 1.0/(PI*at*ab*d*d+EPS);}
vec3 toneMap(vec3 value){vec3 positive=max(value,vec3(0.0));vec3 mapped=positive/(vec3(1.0)+positive);return pow(clamp(mapped,vec3(0.0),vec3(1.0)),vec3(1.0/2.2));}

void main(){
    vec3 base=max(uColor0.rgb,vec3(0.0));
    vec3 ambient=max(uAmbientColor,vec3(0.0));
    vec3 explicitSpecular=max(uColor1.rgb,vec3(0.0));
    vec3 emissive=max(uScalar1.yzw,vec3(0.0));
    vec3 transmissionColor=max(uTransmissionColor,vec3(0.0));
    float opacity=clamp(uColor0.a,0.0,1.0);
    float metallic=clamp(uScalar0.x,0.0,1.0);
    float roughness=clamp(uScalar0.y,0.04,1.0);
    float ior=max(uScalar0.z,1.0001);
    float transmission=clamp(uScalar0.w,0.0,1.0);
    float reflectivity=clamp(uScalar1.x,0.0,1.0);
    float clearcoat=clamp(uScalar2.x,0.0,1.0);
    float clearcoatRoughness=clamp(uScalar2.y,0.04,1.0);
    float sheen=clamp(uScalar2.z,0.0,1.0);
    float anisotropy=clamp(uScalar2.w,-0.95,0.95);

    if(hasT(SB))base*=texture(uBaseColorTex,materialUv(0)).rgb;
    if(hasT(SA))ambient*=texture(uAmbientTex,materialUv(1)).rgb;
    if(hasT(SS))explicitSpecular*=texture(uSpecularTex,materialUv(2)).rgb;
    if(hasT(SE))emissive*=texture(uEmissiveTex,materialUv(3)).rgb;
    if(hasT(SM))metallic=clamp(metallic*scalarSample(uMetallicTex,4),0.0,1.0);
    if(hasT(SR))roughness=clamp(roughness*scalarSample(uRoughnessTex,5),0.04,1.0);
    if(hasT(SO))opacity=clamp(opacity*scalarSample(uOpacityTex,6),0.0,1.0);
    if(hasT(SREF)){vec3 reflectionSample=texture(uReflectionTex,materialUv(9)).rgb;reflectivity*=dot(reflectionSample,vec3(1.0/3.0));}
    if(hasT(STRANS)){vec4 transSample=texture(uTransmissionTex,materialUv(10));transmission=clamp(transmission*channel(transSample,int(uTexOptions[10].y+0.5)),0.0,1.0);transmissionColor*=transSample.rgb;}
    if(hasT(SCC))clearcoat=clamp(clearcoat*scalarSample(uClearcoatTex,11),0.0,1.0);
    if(hasT(SCCR))clearcoatRoughness=clamp(clearcoatRoughness*scalarSample(uClearcoatRoughnessTex,12),0.04,1.0);
    if(hasT(SSH))sheen=clamp(sheen*scalarSample(uSheenTex,13),0.0,1.0);
    if(hasT(SAN))anisotropy=clamp(anisotropy*scalarSample(uAnisotropyTex,14),-0.95,0.95);

    vec3 n=normalize(vNormal);
    vec3 t=normalize(vTangent-n*dot(n,vTangent));
    vec3 b=normalize(cross(n,t))*vSign;
    if(hasT(SBUMP)){
        vec2 uv=materialUv(8);vec2 texel=1.0/vec2(textureSize(uBumpTex,0));
        int ch=int(uTexOptions[8].y+0.5);float strength=uTexOptions[8].x;
        float hL=channel(texture(uBumpTex,uv-vec2(texel.x,0.0)),ch);
        float hR=channel(texture(uBumpTex,uv+vec2(texel.x,0.0)),ch);
        float hD=channel(texture(uBumpTex,uv-vec2(0.0,texel.y)),ch);
        float hU=channel(texture(uBumpTex,uv+vec2(0.0,texel.y)),ch);
        n=normalize(n+t*(hL-hR)*strength+b*(hD-hU)*strength);
        t=normalize(t-n*dot(n,t));b=normalize(cross(n,t))*vSign;
    }
    if(hasT(SN)){
        vec3 tangentNormal=normalize(texture(uNormalTex,materialUv(7)).xyz*2.0-1.0);
        n=normalize(mat3(t,b,n)*tangentNormal);
        t=normalize(t-n*dot(n,t));b=normalize(cross(n,t))*vSign;
    }

    vec3 v=normalize(uCameraPosition-vWorldPosition);
    vec3 lit=emissive+ambient*base*0.025;
    for(int i=0;i<uLightCount;++i){
        LightData light=lights[i];int type=int(light.positionType.w+0.5);
        vec3 l=vec3(0.0),radiance=vec3(0.0);
        if(type==0){l=normalize(-light.directionRange.xyz);radiance=light.colorIntensity.xyz*light.colorIntensity.w;}
        else{
            vec3 toLight=light.positionType.xyz-vWorldPosition;float distanceValue=length(toLight);
            if(distanceValue<=EPS||distanceValue>=light.directionRange.w)continue;
            l=toLight/distanceValue;float ratio=distanceValue/light.directionRange.w;float attenuation=pow(clamp(1.0-ratio*ratio*ratio*ratio,0.0,1.0),2.0)/(distanceValue*distanceValue+1.0);
            float cone=1.0;
            if(type==2){vec3 fromLight=normalize(vWorldPosition-light.positionType.xyz);float cosineValue=dot(normalize(light.directionRange.xyz),fromLight),inner=light.coneShadow.x,outer=light.coneShadow.y;cone=inner<=outer+EPS?(cosineValue>=outer?1.0:0.0):clamp((cosineValue-outer)/(inner-outer),0.0,1.0);}
            if(cone<=0.0)continue;radiance=light.colorIntensity.xyz*light.colorIntensity.w*attenuation*cone;
        }
        float nl=max(dot(n,l),0.0),nv=max(dot(n,v),0.0);if(nl<=0.0||nv<=0.0)continue;
        vec3 h=normalize(v+l);
        vec3 dielectricF0=dot(explicitSpecular,explicitSpecular)>EPS?clamp(explicitSpecular,vec3(0.0),vec3(1.0)):vec3(iorToF0(ior));
        vec3 f0=mix(dielectricF0,base,metallic),f=fresnelSchlick(max(dot(h,v),0.0),f0);
        float D=abs(anisotropy)>0.001?distributionAnisotropic(n,t,b,h,roughness,anisotropy):distributionGgx(n,h,roughness);
        float G=geometrySmith(n,v,l,roughness);
        vec3 specular=f*D*G/(4.0*nv*nl+EPS);
        vec3 diffuse=(vec3(1.0)-f)*(1.0-metallic)*(base/PI);
        vec3 brdf=diffuse+specular;
        if(clearcoat>0.0){vec3 cf=fresnelSchlick(max(dot(h,v),0.0),vec3(0.04));float cD=distributionGgx(n,h,clearcoatRoughness),cG=geometrySmith(n,v,l,clearcoatRoughness);vec3 coat=cf*cD*cG/(4.0*nv*nl+EPS);brdf=brdf*(vec3(1.0)-cf*clearcoat)+coat*clearcoat;}
        float sheenTerm=sheen*(1.0-metallic)*pow(1.0-max(dot(h,v),0.0),5.0);brdf+=mix(vec3(1.0),base,0.5)*sheenTerm;
        lit+=brdf*radiance*nl;
    }

    vec2 screen=gl_FragCoord.xy/uViewport;
    float eta=1.0/ior;
    vec2 refractionOffset=n.xy*(1.0-eta)*0.035*(0.25+0.75*transmission);
    vec3 behind=texture(sOpaque,clamp(screen+refractionOffset,vec2(0.0),vec2(1.0))).rgb;
    float dielectric=iorToF0(ior);
    float fresnel=dielectric+(1.0-dielectric)*pow(1.0-clamp(dot(n,v),0.0,1.0),5.0);
    vec3 litLdr=toneMap(lit);
    vec3 transmitted=behind*transmissionColor*(1.0-fresnel)*transmission;
    vec3 reflected=mix(litLdr,base,reflectivity)*fresnel*(1.0+clearcoat*0.25);
    vec3 surface=litLdr*(1.0-transmission)+transmitted+reflected;
    float alpha=clamp(opacity,0.0,1.0);
    oColor=vec4(surface*alpha,alpha);
}
)GLSL";

void errorOut(std::string *error,const char *message){if(error)*error=message?message:"transparent GPU error";}
int channelCode(char c){if(c=='g'||c=='G')return 1;if(c=='b'||c=='B')return 2;if(c=='a'||c=='A')return 3;if(c=='m'||c=='M'||c=='l'||c=='L')return 4;return 0;}

} // namespace

bool TransparentGpu::init(std::string *error)
{
    shutdown();program_=createGraphicsProgram(VERTEX_SHADER,FRAGMENT_SHADER,error);if(program_==0)return false;
    view_location_=GL20.glGetUniformLocation(program_,"uView");projection_location_=GL20.glGetUniformLocation(program_,"uProjection");model_location_=GL20.glGetUniformLocation(program_,"uModel");normal_location_=GL20.glGetUniformLocation(program_,"uNormalMatrix");camera_location_=GL20.glGetUniformLocation(program_,"uCameraPosition");viewport_location_=GL20.glGetUniformLocation(program_,"uViewport");light_count_location_=GL20.glGetUniformLocation(program_,"uLightCount");texture_mask_location_=GL20.glGetUniformLocation(program_,"uTextureMask");scalar0_location_=GL20.glGetUniformLocation(program_,"uScalar0");scalar1_location_=GL20.glGetUniformLocation(program_,"uScalar1");scalar2_location_=GL20.glGetUniformLocation(program_,"uScalar2");color0_location_=GL20.glGetUniformLocation(program_,"uColor0");color1_location_=GL20.glGetUniformLocation(program_,"uColor1");ambient_location_=GL20.glGetUniformLocation(program_,"uAmbientColor");transmission_color_location_=GL20.glGetUniformLocation(program_,"uTransmissionColor");
    if(view_location_<0||projection_location_<0||model_location_<0||normal_location_<0||camera_location_<0||viewport_location_<0||light_count_location_<0||texture_mask_location_<0||scalar0_location_<0||scalar1_location_<0||scalar2_location_<0||color0_location_<0||color1_location_<0||ambient_location_<0||transmission_color_location_<0){errorOut(error,"transparent shader uniforms unavailable");shutdown();return false;}
    static const char* names[15]={"uBaseColorTex","uAmbientTex","uSpecularTex","uEmissiveTex","uMetallicTex","uRoughnessTex","uOpacityTex","uNormalTex","uBumpTex","uReflectionTex","uTransmissionTex","uClearcoatTex","uClearcoatRoughnessTex","uSheenTex","uAnisotropyTex"};
    for(std::size_t i=0;i<15;++i){char name[64];sampler_locations_[i]=GL20.glGetUniformLocation(program_,names[i]);std::snprintf(name,sizeof(name),"uTexScaleOffset[%zu]",i);tex_scale_offset_locations_[i]=GL20.glGetUniformLocation(program_,name);std::snprintf(name,sizeof(name),"uTexOptions[%zu]",i);tex_options_locations_[i]=GL20.glGetUniformLocation(program_,name);if(sampler_locations_[i]<0||tex_scale_offset_locations_[i]<0||tex_options_locations_[i]<0){errorOut(error,"transparent texture uniforms unavailable");shutdown();return false;}}
    if(!material_gpu_.init(error))return false;
    GL20.glUseProgram(program_);for(int i=0;i<15;++i)GL20.glUniform1i(sampler_locations_[i],i+1);GL20.glUseProgram(0);
    GL30.glGenFramebuffers(1,&framebuffer_);GL30.glGenFramebuffers(1,&copy_framebuffer_);if(framebuffer_==0||copy_framebuffer_==0){errorOut(error,"failed to allocate transparent framebuffers");shutdown();return false;}
    mesh_revision_=Mesh::loadedMeshRevision();if(error)error->clear();return true;
}

bool TransparentGpu::allocateTarget(std::string *error){if(final_color_==0)final_color_=lwcgl_glGenTexture();if(final_color_==0){errorOut(error,"failed to allocate transparent color target");return false;}glBindTexture(GL_TEXTURE_2D,final_color_);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width_,height_,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glBindTexture(GL_TEXTURE_2D,0);if(error)error->clear();return true;}
bool TransparentGpu::resize(int width,int height,std::string *error){width=normalizedExtent(width);height=normalizedExtent(height);if(width==width_&&height==height_&&final_color_!=0)return true;width_=width;height_=height;return allocateTarget(error);}

TransparentGpu::MeshGpu* TransparentGpu::meshGpu(std::uint32_t handle,std::string *error){for(auto& mesh:meshes_)if(mesh.handle==handle)return &mesh;const Mesh::MeshData* source=Mesh::loadedMesh(handle);if(!source||source->vertices.empty()||source->indices.empty()){errorOut(error,"transparent loaded mesh unavailable");return nullptr;}MeshGpu mesh;mesh.handle=handle;GL30.glGenVertexArrays(1,&mesh.vao);GL15.glGenBuffers(1,&mesh.vbo);GL15.glGenBuffers(1,&mesh.ebo);if(!mesh.vao||!mesh.vbo||!mesh.ebo){destroyMesh(&mesh);errorOut(error,"failed to allocate transparent mesh resources");return nullptr;}GL30.glBindVertexArray(mesh.vao);GL15.glBindBuffer(GL_ARRAY_BUFFER,mesh.vbo);GL15.glBufferData(GL_ARRAY_BUFFER,static_cast<LWCGLsizeiptr>(source->vertices.size()*sizeof(Mesh::Vertex)),source->vertices.data(),GL_STATIC_DRAW);GL15.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mesh.ebo);GL15.glBufferData(GL_ELEMENT_ARRAY_BUFFER,static_cast<LWCGLsizeiptr>(source->indices.size()*sizeof(std::uint32_t)),source->indices.data(),GL_STATIC_DRAW);GL20.glEnableVertexAttribArray(0);GL20.glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Mesh::Vertex),reinterpret_cast<void*>(offsetof(Mesh::Vertex,position)));GL20.glEnableVertexAttribArray(1);GL20.glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Mesh::Vertex),reinterpret_cast<void*>(offsetof(Mesh::Vertex,normal)));GL20.glEnableVertexAttribArray(2);GL20.glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Mesh::Vertex),reinterpret_cast<void*>(offsetof(Mesh::Vertex,uv)));GL20.glEnableVertexAttribArray(3);GL20.glVertexAttribPointer(3,4,GL_FLOAT,GL_FALSE,sizeof(Mesh::Vertex),reinterpret_cast<void*>(offsetof(Mesh::Vertex,tangent)));GL30.glBindVertexArray(0);mesh.index_count=static_cast<GLsizei>(source->indices.size());meshes_.push_back(mesh);return &meshes_.back();}

bool TransparentGpu::updateScene(const Ecs::World& world,std::string *error){if(mesh_revision_!=Mesh::loadedMeshRevision()){clearMeshes();mesh_revision_=Mesh::loadedMeshRevision();}items_.clear();for(const Ecs::Entity entity:world.entities()){const auto* transform=world.getTransform(entity);const auto* mesh=world.getMesh(entity);const auto* renderable=world.getRenderable(entity);const auto* material=world.getMaterial(entity);if(!transform||!mesh||!renderable||!renderable->visible||!material||mesh->loaded_mesh==Ecs::INVALID_ASSET_HANDLE||material->renderer_material==Ecs::INVALID_ASSET_HANDLE)continue;const Material::Resource* resource=Material::get(material->renderer_material);if(!resource||(resource->render_class!=Material::RenderClass::Transparent&&resource->render_class!=Material::RenderClass::Transmissive))continue;const Mesh::MeshData* data=Mesh::loadedMesh(mesh->loaded_mesh);if(!data)continue;Item item;item.mesh=mesh->loaded_mesh;item.material=material->renderer_material;const Math::Vec3 position={transform->position.x,transform->position.y,transform->position.z},rotation={transform->rotation.x,transform->rotation.y,transform->rotation.z},scale={transform->scale.x,transform->scale.y,transform->scale.z};item.model=Math::transform(position,rotation,scale);const auto inverse=[](float value){return std::fabs(value)>1.0e-6f?1.0f/value:(value<0.0f?-1000000.0f:1000000.0f);};item.normal=Math::multiply(Math::rotationEuler(rotation),Math::scaling({inverse(scale.x),inverse(scale.y),inverse(scale.z)}));const Math::Vec3 local={(data->bounds.minimum.x+data->bounds.maximum.x)*0.5f,(data->bounds.minimum.y+data->bounds.maximum.y)*0.5f,(data->bounds.minimum.z+data->bounds.maximum.z)*0.5f};item.center=Math::transformPoint(item.model,local);items_.push_back(item);if(!meshGpu(item.mesh,error))return false;}if(error)error->clear();return true;}

bool TransparentGpu::bindMaterial(Material::MaterialHandle handle,std::string *error){const Material::Resource* resource=Material::get(handle);if(!resource){errorOut(error,"invalid transparent material");return false;}if(!material_gpu_.bind(handle,1,error))return false;GL20.glUniform1i(texture_mask_location_,static_cast<GLint>(material_gpu_.textureMask(handle)));GL20.glUniform4f(scalar0_location_,resource->metallic,resource->roughness,resource->ior,resource->transmission);GL20.glUniform4f(scalar1_location_,resource->reflectivity,resource->emissive.x,resource->emissive.y,resource->emissive.z);GL20.glUniform4f(scalar2_location_,resource->clearcoat,resource->clearcoat_roughness,resource->sheen,resource->anisotropy);GL20.glUniform4f(color0_location_,resource->base_color.x,resource->base_color.y,resource->base_color.z,resource->opacity);GL20.glUniform4f(color1_location_,resource->specular.x*resource->specular_strength,resource->specular.y*resource->specular_strength,resource->specular.z*resource->specular_strength,1.0f);GL20.glUniform3f(ambient_location_,resource->ambient.x,resource->ambient.y,resource->ambient.z);GL20.glUniform3f(transmission_color_location_,resource->transmission_color.x,resource->transmission_color.y,resource->transmission_color.z);for(std::size_t i=0;i<15;++i){const auto slot=MaterialGpu::liveSlot(i);const auto& binding=resource->textures[Material::slotIndex(slot)];GL20.glUniform4f(tex_scale_offset_locations_[i],binding.scale.x,binding.scale.y,binding.offset.x+binding.turbulence.x,binding.offset.y+binding.turbulence.y);GL20.glUniform4f(tex_options_locations_[i],binding.multiplier,static_cast<float>(channelCode(binding.channel)),0.0f,0.0f);}return true;}

bool TransparentGpu::render(const Ecs::World&,const GBufferGpu& gbuffer,const DirectLightingGpu& direct,GLuint opaque,const Math::Mat4& view,const Math::Mat4& projection,const Math::Vec3& camera,std::string *error){last_output_=opaque;if(items_.empty()){if(error)error->clear();return true;}if(!final_color_||!framebuffer_||!copy_framebuffer_){errorOut(error,"transparent targets not ready");return false;}GL30.glBindFramebuffer(GL_READ_FRAMEBUFFER,copy_framebuffer_);GL30.glFramebufferTexture2D(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,opaque,0);GL30.glBindFramebuffer(GL_DRAW_FRAMEBUFFER,framebuffer_);GL30.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,final_color_,0);GL30.glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,gbuffer.depthTexture(),0);GL30.glBlitFramebuffer(0,0,width_,height_,0,0,width_,height_,GL_COLOR_BUFFER_BIT,GL_NEAREST);GL30.glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_);glViewport(0,0,width_,height_);for(Item& item:items_){const Math::Vec3 viewCenter=Math::transformPoint(view,item.center);item.depth=-viewCenter.z;}std::stable_sort(items_.begin(),items_.end(),[](const Item& a,const Item& b){return a.depth>b.depth;});glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);glEnable(GL_BLEND);glBlendFunc(GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_CULL_FACE);GL20.glUseProgram(program_);GL20.glUniformMatrix4fv(view_location_,1,GL_FALSE,view.value);GL20.glUniformMatrix4fv(projection_location_,1,GL_FALSE,projection.value);GL20.glUniform3f(camera_location_,camera.x,camera.y,camera.z);GL20.glUniform2f(viewport_location_,static_cast<float>(width_),static_cast<float>(height_));GL20.glUniform1i(light_count_location_,static_cast<GLint>(direct.lightCount()));GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,5,direct.lightBuffer());GLModern.glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,opaque);for(const Item& item:items_){MeshGpu* mesh=meshGpu(item.mesh,error);if(!mesh||!bindMaterial(item.material,error)){GL20.glUseProgram(0);glDepthMask(GL_TRUE);glDisable(GL_BLEND);GL30.glBindFramebuffer(GL_FRAMEBUFFER,0);return false;}GL20.glUniformMatrix4fv(model_location_,1,GL_FALSE,item.model.value);GL20.glUniformMatrix4fv(normal_location_,1,GL_FALSE,item.normal.value);GL30.glBindVertexArray(mesh->vao);glDrawElements(GL_TRIANGLES,mesh->index_count,GL_UNSIGNED_INT,nullptr);}GL30.glBindVertexArray(0);GL20.glUseProgram(0);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);GL30.glBindFramebuffer(GL_FRAMEBUFFER,0);last_output_=final_color_;if(error)error->clear();return true;}

void TransparentGpu::destroyMesh(MeshGpu* mesh){if(!mesh)return;if(mesh->ebo)GL15.glDeleteBuffers(1,&mesh->ebo);if(mesh->vbo)GL15.glDeleteBuffers(1,&mesh->vbo);if(mesh->vao)GL30.glDeleteVertexArrays(1,&mesh->vao);*mesh={};}
void TransparentGpu::clearMeshes(){for(auto& mesh:meshes_)destroyMesh(&mesh);meshes_.clear();}
void TransparentGpu::shutdown(){clearMeshes();items_.clear();material_gpu_.shutdown();if(final_color_)glDeleteTextures(final_color_);final_color_=0;last_output_=0;if(framebuffer_)GL30.glDeleteFramebuffers(1,&framebuffer_);if(copy_framebuffer_)GL30.glDeleteFramebuffers(1,&copy_framebuffer_);framebuffer_=copy_framebuffer_=0;destroyProgram(&program_);mesh_revision_=0u;width_=height_=0;}

} }
