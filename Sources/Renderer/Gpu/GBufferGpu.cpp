#include "GBufferGpu.hpp"

#include "Renderer/Gpu/DirtyRanges.hpp"
#include "Renderer/Gpu/Gpu.hpp"
#include "Renderer/Gpu/ResourceLifecycle.hpp"
#include "Renderer/Gpu/SurfaceFormats.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Renderer { namespace Gpu { namespace {

constexpr const char* GBUFFER_VERTEX_SHADER=R"GLSL(
#version 430 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUv;
layout(location=3) in vec4 aTangent;
struct InstanceData {
    mat4 model;
    mat4 normalMatrix;
    vec4 albedoMetallic;
    vec4 emissiveRoughness;
    vec4 ambientOpacity;
    vec4 specularIor;
    vec4 advanced;
    vec4 transmission;
    vec4 extra;
};
layout(std430,binding=0) readonly buffer InstanceBuffer { InstanceData instances[]; };
uniform mat4 uView;
uniform mat4 uProjection;
out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec3 vWorldTangent;
out vec2 vUv;
flat out int vInstanceId;
flat out float vTangentSign;
void main(){
    InstanceData instance=instances[gl_InstanceID];
    vec4 world=instance.model*vec4(aPosition,1.0);
    vWorldPosition=world.xyz;
    vWorldNormal=normalize(mat3(instance.normalMatrix)*aNormal);
    vWorldTangent=normalize(mat3(instance.normalMatrix)*aTangent.xyz);
    vTangentSign=aTangent.w;
    vUv=aUv;
    vInstanceId=gl_InstanceID;
    gl_Position=uProjection*uView*world;
}
)GLSL";

constexpr const char* GBUFFER_FRAGMENT_SHADER=R"GLSL(
#version 430 core
in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec3 vWorldTangent;
in vec2 vUv;
flat in int vInstanceId;
flat in float vTangentSign;
struct InstanceData {
    mat4 model;
    mat4 normalMatrix;
    vec4 albedoMetallic;
    vec4 emissiveRoughness;
    vec4 ambientOpacity;
    vec4 specularIor;
    vec4 advanced;
    vec4 transmission;
    vec4 extra;
};
layout(std430,binding=0) readonly buffer InstanceBuffer { InstanceData instances[]; };

uniform int uTextureMask;
uniform float uAlphaCutoff;
uniform int uMasked;
uniform vec4 uTexScaleOffset[15];
uniform vec4 uTexOptions[15];
uniform sampler2D uBaseColorTex;
uniform sampler2D uAmbientTex;
uniform sampler2D uSpecularTex;
uniform sampler2D uEmissiveTex;
uniform sampler2D uMetallicTex;
uniform sampler2D uRoughnessTex;
uniform sampler2D uOpacityTex;
uniform sampler2D uNormalTex;
uniform sampler2D uBumpTex;
uniform sampler2D uReflectionTex;
uniform sampler2D uTransmissionTex;
uniform sampler2D uClearcoatTex;
uniform sampler2D uClearcoatRoughnessTex;
uniform sampler2D uSheenTex;
uniform sampler2D uAnisotropyTex;

layout(location=0) out vec4 oPositionDepth;
layout(location=1) out vec4 oNormalRoughness;
layout(location=2) out vec4 oAlbedoMetallic;
layout(location=3) out vec4 oEmissiveOpacity;
layout(location=4) out vec4 oSpecularIor;
layout(location=5) out vec4 oAdvanced;
layout(location=6) out vec4 oAmbientTransmission;
layout(location=7) out vec4 oTangentAnisotropy;

const int SLOT_BASE_COLOR=0;
const int SLOT_AMBIENT=1;
const int SLOT_SPECULAR=2;
const int SLOT_EMISSIVE=3;
const int SLOT_METALLIC=4;
const int SLOT_ROUGHNESS=5;
const int SLOT_OPACITY=7;
const int SLOT_NORMAL=8;
const int SLOT_BUMP=9;
const int SLOT_REFLECTION=11;
const int SLOT_TRANSMISSION=12;
const int SLOT_CLEARCOAT=13;
const int SLOT_CLEARCOAT_ROUGHNESS=14;
const int SLOT_SHEEN=15;
const int SLOT_ANISOTROPY=16;

bool hasTexture(int slot){return (uTextureMask & (1 << slot)) != 0;}
vec2 uvFor(int liveIndex){vec4 t=uTexScaleOffset[liveIndex];return vUv*t.xy+t.zw;}
float channelValue(vec4 s,int channel){if(channel==1)return s.g;if(channel==2)return s.b;if(channel==3)return s.a;if(channel==4)return dot(s.rgb,vec3(1.0/3.0));return s.r;}
float scalarSample(sampler2D tex,int liveIndex){return channelValue(texture(tex,uvFor(liveIndex)),int(uTexOptions[liveIndex].y+0.5));}

void main(){
    InstanceData inst=instances[vInstanceId];
    vec3 base=max(inst.albedoMetallic.rgb,vec3(0.0));
    float metallic=clamp(inst.albedoMetallic.a,0.0,1.0);
    vec3 emissive=max(inst.emissiveRoughness.rgb,vec3(0.0));
    float roughness=clamp(inst.emissiveRoughness.a,0.04,1.0);
    vec3 ambient=max(inst.ambientOpacity.rgb,vec3(0.0));
    float opacity=clamp(inst.ambientOpacity.a,0.0,1.0);
    vec3 specular=max(inst.specularIor.rgb,vec3(0.0));
    float ior=max(inst.specularIor.a,1.0001);
    float clearcoat=clamp(inst.advanced.x,0.0,1.0);
    float clearcoatRoughness=clamp(inst.advanced.y,0.04,1.0);
    float sheen=clamp(inst.advanced.z,0.0,1.0);
    float reflectivity=clamp(inst.advanced.w,0.0,1.0);
    vec3 transmissionColor=max(inst.transmission.rgb,vec3(0.0));
    float transmission=clamp(inst.transmission.a,0.0,1.0);
    float anisotropy=clamp(inst.extra.x,-1.0,1.0);

    if(hasTexture(SLOT_BASE_COLOR)) base*=texture(uBaseColorTex,uvFor(0)).rgb;
    if(hasTexture(SLOT_AMBIENT)) ambient*=texture(uAmbientTex,uvFor(1)).rgb;
    if(hasTexture(SLOT_SPECULAR)) specular*=texture(uSpecularTex,uvFor(2)).rgb;
    if(hasTexture(SLOT_EMISSIVE)) emissive*=texture(uEmissiveTex,uvFor(3)).rgb;
    if(hasTexture(SLOT_METALLIC)) metallic*=scalarSample(uMetallicTex,4);
    if(hasTexture(SLOT_ROUGHNESS)) roughness*=scalarSample(uRoughnessTex,5);
    if(hasTexture(SLOT_OPACITY)) opacity*=scalarSample(uOpacityTex,6);
    if(hasTexture(SLOT_REFLECTION)) reflectivity*=scalarSample(uReflectionTex,9);
    if(hasTexture(SLOT_TRANSMISSION)){
        vec4 t=texture(uTransmissionTex,uvFor(10));
        transmissionColor*=t.rgb;
        transmission*=channelValue(t,int(uTexOptions[10].y+0.5));
    }
    if(hasTexture(SLOT_CLEARCOAT)) clearcoat*=scalarSample(uClearcoatTex,11);
    if(hasTexture(SLOT_CLEARCOAT_ROUGHNESS)) clearcoatRoughness*=scalarSample(uClearcoatRoughnessTex,12);
    if(hasTexture(SLOT_SHEEN)) sheen*=scalarSample(uSheenTex,13);
    if(hasTexture(SLOT_ANISOTROPY)) anisotropy*=((scalarSample(uAnisotropyTex,14)*2.0)-1.0);

    if(uMasked!=0 && opacity<uAlphaCutoff) discard;

    vec3 n=normalize(vWorldNormal);
    vec3 t=normalize(vWorldTangent-n*dot(n,vWorldTangent));
    vec3 b=normalize(cross(n,t))*vTangentSign;
    mat3 tbn=mat3(t,b,n);
    vec3 tangentNormal=vec3(0.0,0.0,1.0);
    if(hasTexture(SLOT_NORMAL)) tangentNormal=normalize(texture(uNormalTex,uvFor(7)).xyz*2.0-1.0);
    if(hasTexture(SLOT_BUMP)){
        vec2 uv=uvFor(8);
        vec2 texel=1.0/vec2(textureSize(uBumpTex,0));
        int ch=int(uTexOptions[8].y+0.5);
        float h=channelValue(texture(uBumpTex,uv),ch);
        float hx=channelValue(texture(uBumpTex,uv+vec2(texel.x,0.0)),ch);
        float hy=channelValue(texture(uBumpTex,uv+vec2(0.0,texel.y)),ch);
        float strength=uTexOptions[8].x;
        vec3 bumpN=normalize(vec3((h-hx)*strength,(h-hy)*strength,1.0));
        tangentNormal=normalize(vec3(tangentNormal.xy+bumpN.xy,tangentNormal.z*bumpN.z));
    }
    n=normalize(tbn*tangentNormal);

    oPositionDepth=vec4(vWorldPosition,gl_FragCoord.z);
    oNormalRoughness=vec4(n,clamp(roughness,0.04,1.0));
    oAlbedoMetallic=vec4(base,clamp(metallic,0.0,1.0));
    oEmissiveOpacity=vec4(emissive,opacity);
    oSpecularIor=vec4(specular,ior);
    oAdvanced=vec4(clearcoat,clearcoatRoughness,sheen,reflectivity);
    oAmbientTransmission=vec4(ambient,transmission);
    oTangentAnisotropy=vec4(t,anisotropy);
}
)GLSL";

Math::Vec3 toVec3(const Ecs::Vec3& v){ return {v.x,v.y,v.z}; }
float safeInverse(float v){ if(std::fabs(v)<0.00001f) return v<0.0f?-100000.0f:100000.0f; return 1.0f/v; }
GLint surfaceInternalFormat(SurfaceFormat f){ return f==SurfaceFormat::Rgba8?GL_RGBA8:GL_RGBA16F; }
GLenum surfacePixelType(SurfaceFormat f){ return f==SurfaceFormat::Rgba8?GL_UNSIGNED_BYTE:GL_FLOAT; }
void setError(std::string* error,const char* message){ if(error) *error=message?message:"GPU GBuffer error"; }

int channelCode(char channel)
{
    switch(channel){case 'g':case 'G':return 1;case 'b':case 'B':return 2;case 'a':case 'A':return 3;case 'm':case 'M':return 4;default:return 0;}
}

bool ensureColorTexture(GLuint* texture,int width,int height,SurfaceFormat format)
{
    if(!texture) return false;
    if(*texture==0) *texture=lwcgl_glGenTexture();
    if(*texture==0) return false;
    glBindTexture(GL_TEXTURE_2D,*texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,surfaceInternalFormat(format),width,height,0,GL_RGBA,surfacePixelType(format),nullptr);
    return true;
}

bool ensureDepthTexture(GLuint* texture,int width,int height)
{
    if(!texture) return false;
    if(*texture==0) *texture=lwcgl_glGenTexture();
    if(*texture==0) return false;
    glBindTexture(GL_TEXTURE_2D,*texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24,width,height,0,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,nullptr);
    return true;
}
void deleteTexture(GLuint* texture){ if(texture && *texture!=0){ glDeleteTextures(*texture); *texture=0; } }

} // namespace

bool GBufferGpu::init(std::string* error)
{
    shutdown();
    program_=createGraphicsProgram(GBUFFER_VERTEX_SHADER,GBUFFER_FRAGMENT_SHADER,error);
    if(program_==0) return false;
    view_location_=GL20.glGetUniformLocation(program_,"uView");
    projection_location_=GL20.glGetUniformLocation(program_,"uProjection");
    texture_mask_location_=GL20.glGetUniformLocation(program_,"uTextureMask");
    alpha_cutoff_location_=GL20.glGetUniformLocation(program_,"uAlphaCutoff");
    masked_location_=GL20.glGetUniformLocation(program_,"uMasked");
    if(view_location_<0||projection_location_<0||texture_mask_location_<0||alpha_cutoff_location_<0||masked_location_<0){setError(error,"GPU GBuffer uniforms are unavailable");shutdown();return false;}
    static const char* sampler_names[MaterialGpu::LIVE_TEXTURE_COUNT]={"uBaseColorTex","uAmbientTex","uSpecularTex","uEmissiveTex","uMetallicTex","uRoughnessTex","uOpacityTex","uNormalTex","uBumpTex","uReflectionTex","uTransmissionTex","uClearcoatTex","uClearcoatRoughnessTex","uSheenTex","uAnisotropyTex"};
    for(std::size_t i=0;i<MaterialGpu::LIVE_TEXTURE_COUNT;++i){char name[64];sampler_locations_[i]=GL20.glGetUniformLocation(program_,sampler_names[i]);std::snprintf(name,sizeof(name),"uTexScaleOffset[%zu]",i);tex_scale_offset_locations_[i]=GL20.glGetUniformLocation(program_,name);std::snprintf(name,sizeof(name),"uTexOptions[%zu]",i);tex_options_locations_[i]=GL20.glGetUniformLocation(program_,name);if(sampler_locations_[i]<0||tex_scale_offset_locations_[i]<0||tex_options_locations_[i]<0){setError(error,"GPU GBuffer material uniforms are unavailable");shutdown();return false;}}
    if(!material_gpu_.init(error)) { shutdown(); return false; }
    GL20.glUseProgram(program_); for(std::size_t i=0;i<MaterialGpu::LIVE_TEXTURE_COUNT;++i) GL20.glUniform1i(sampler_locations_[i],static_cast<GLint>(i)); GL20.glUseProgram(0);
    if(!createMesh(Ecs::MeshType::Cube,&cubes_.mesh,error)||!createMesh(Ecs::MeshType::Plane,&planes_.mesh,error)){shutdown();return false;}
    GL15.glGenBuffers(1,&cubes_.instance_buffer);GL15.glGenBuffers(1,&planes_.instance_buffer);
    if(cubes_.instance_buffer==0||planes_.instance_buffer==0){setError(error,"failed to allocate GPU GBuffer instance buffers");shutdown();return false;}
    if(error) error->clear();
    return true;
}

bool GBufferGpu::resize(int width,int height,std::string* error){if(!resizeStorageRequired(width_,height_,framebuffer_!=0,width,height))return true;width_=normalizedExtent(width);height_=normalizedExtent(height);return createAttachments(error);}
bool GBufferGpu::createMesh(Ecs::MeshType type,MeshGpu* mesh,std::string* error){return createMesh(Mesh::meshForType(type),mesh,error);}

bool GBufferGpu::createMesh(const Mesh::MeshData& source,MeshGpu* mesh,std::string* error)
{
    if(!mesh){setError(error,"null GPU mesh destination");return false;} if(source.vertices.empty()||source.indices.empty()){setError(error,"cannot upload an empty renderer mesh");return false;}
    GL30.glGenVertexArrays(1,&mesh->vao);GL15.glGenBuffers(1,&mesh->vertex_buffer);GL15.glGenBuffers(1,&mesh->index_buffer);if(mesh->vao==0||mesh->vertex_buffer==0||mesh->index_buffer==0){setError(error,"failed to allocate persistent GPU mesh resources");destroyMesh(mesh);return false;}
    GL30.glBindVertexArray(mesh->vao);GL15.glBindBuffer(GL_ARRAY_BUFFER,mesh->vertex_buffer);GL15.glBufferData(GL_ARRAY_BUFFER,static_cast<LWCGLsizeiptr>(source.vertices.size()*sizeof(Mesh::Vertex)),source.vertices.data(),GL_STATIC_DRAW);GL15.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mesh->index_buffer);GL15.glBufferData(GL_ELEMENT_ARRAY_BUFFER,static_cast<LWCGLsizeiptr>(source.indices.size()*sizeof(std::uint32_t)),source.indices.data(),GL_STATIC_DRAW);
    GL20.glEnableVertexAttribArray(0);GL20.glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,static_cast<GLsizei>(sizeof(Mesh::Vertex)),reinterpret_cast<const void*>(offsetof(Mesh::Vertex,position)));
    GL20.glEnableVertexAttribArray(1);GL20.glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,static_cast<GLsizei>(sizeof(Mesh::Vertex)),reinterpret_cast<const void*>(offsetof(Mesh::Vertex,normal)));
    GL20.glEnableVertexAttribArray(2);GL20.glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,static_cast<GLsizei>(sizeof(Mesh::Vertex)),reinterpret_cast<const void*>(offsetof(Mesh::Vertex,uv)));
    GL20.glEnableVertexAttribArray(3);GL20.glVertexAttribPointer(3,4,GL_FLOAT,GL_FALSE,static_cast<GLsizei>(sizeof(Mesh::Vertex)),reinterpret_cast<const void*>(offsetof(Mesh::Vertex,tangent)));
    GL30.glBindVertexArray(0);GL15.glBindBuffer(GL_ARRAY_BUFFER,0);GL15.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);mesh->index_count=static_cast<GLsizei>(source.indices.size());return true;
}

GBufferGpu::MeshGpu* GBufferGpu::loadedMeshGpu(std::uint32_t handle,std::string* error)
{
    if(handle==Ecs::INVALID_ASSET_HANDLE){setError(error,"invalid loaded mesh handle");return nullptr;} if(handle>=loaded_meshes_.size()) loaded_meshes_.resize(static_cast<std::size_t>(handle)+1u); MeshGpu& gpu=loaded_meshes_[handle]; if(gpu.vao==0){const Mesh::MeshData* source=Mesh::loadedMesh(handle);if(!source){setError(error,"loaded mesh handle is not registered");return nullptr;}if(!createMesh(*source,&gpu,error))return nullptr;} return &gpu;
}

GBufferGpu::LoadedBatch* GBufferGpu::loadedBatch(std::uint32_t handle,Material::MaterialHandle material,Material::RenderClass render_class,std::string* error)
{
    if(!loadedMeshGpu(handle,error)) return nullptr;
    for(LoadedBatch& existing:loaded_batches_)
    {
        if(existing.mesh_handle==handle && existing.material==material && existing.render_class==render_class)
            return &existing;
    }
    LoadedBatch batch;
    batch.mesh_handle=handle;
    batch.material=material;
    batch.render_class=render_class;
    GL15.glGenBuffers(1,&batch.instance_buffer);
    if(batch.instance_buffer==0)
    {
        setError(error,"failed to allocate loaded-mesh instance buffer");
        return nullptr;
    }
    loaded_batches_.push_back(std::move(batch));
    return &loaded_batches_.back();
}

bool GBufferGpu::uploadBatch(Batch* b,std::string* error)
{
    if(!b||b->instance_buffer==0){setError(error,"invalid GPU GBuffer batch");return false;} if(b->instances.empty()){b->uploaded_instances.clear();return true;} const std::size_t required=b->instances.size()*sizeof(InstanceGpu);GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER,b->instance_buffer);const bool full=required>b->instance_capacity||b->instances.size()!=b->uploaded_instances.size();if(required>b->instance_capacity){std::size_t cap=std::max<std::size_t>(sizeof(InstanceGpu)*16u,b->instance_capacity);while(cap<required)cap*=2u;GL15.glBufferData(GL_SHADER_STORAGE_BUFFER,static_cast<LWCGLsizeiptr>(cap),nullptr,GL_DYNAMIC_DRAW);b->instance_capacity=cap;}if(full)GL15.glBufferSubData(GL_SHADER_STORAGE_BUFFER,0,static_cast<LWCGLsizeiptr>(required),b->instances.data());else forEachDirtyRange(b->instances,b->uploaded_instances,[&](std::size_t first,std::size_t count){GL15.glBufferSubData(GL_SHADER_STORAGE_BUFFER,static_cast<LWCGLintptr>(first*sizeof(InstanceGpu)),static_cast<LWCGLsizeiptr>(count*sizeof(InstanceGpu)),b->instances.data()+first);});b->uploaded_instances=b->instances;return true;
}

bool GBufferGpu::uploadBatch(LoadedBatch* b,std::string* error)
{
    if(!b||b->instance_buffer==0){setError(error,"invalid loaded GPU GBuffer batch");return false;} if(b->instances.empty()){b->uploaded_instances.clear();return true;} const std::size_t required=b->instances.size()*sizeof(InstanceGpu);GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER,b->instance_buffer);const bool full=required>b->instance_capacity||b->instances.size()!=b->uploaded_instances.size();if(required>b->instance_capacity){std::size_t cap=std::max<std::size_t>(sizeof(InstanceGpu)*16u,b->instance_capacity);while(cap<required)cap*=2u;GL15.glBufferData(GL_SHADER_STORAGE_BUFFER,static_cast<LWCGLsizeiptr>(cap),nullptr,GL_DYNAMIC_DRAW);b->instance_capacity=cap;}if(full)GL15.glBufferSubData(GL_SHADER_STORAGE_BUFFER,0,static_cast<LWCGLsizeiptr>(required),b->instances.data());else forEachDirtyRange(b->instances,b->uploaded_instances,[&](std::size_t first,std::size_t count){GL15.glBufferSubData(GL_SHADER_STORAGE_BUFFER,static_cast<LWCGLintptr>(first*sizeof(InstanceGpu)),static_cast<LWCGLsizeiptr>(count*sizeof(InstanceGpu)),b->instances.data()+first);});b->uploaded_instances=b->instances;return true;
}

bool GBufferGpu::bindMaterial(Material::MaterialHandle handle,std::string* error)
{
    if(handle==Material::INVALID_MATERIAL){GL20.glUniform1i(texture_mask_location_,0);GL20.glUniform1f(alpha_cutoff_location_,0.5f);GL20.glUniform1i(masked_location_,0);return true;}
    const Material::Resource* r=Material::get(handle);if(!r){setError(error,"invalid renderer material handle in GBuffer");return false;} if(!material_gpu_.bind(handle,0,error))return false;GL20.glUniform1i(texture_mask_location_,static_cast<GLint>(material_gpu_.textureMask(handle)));GL20.glUniform1f(alpha_cutoff_location_,r->alpha_cutoff);GL20.glUniform1i(masked_location_,r->render_class==Material::RenderClass::Masked?1:0);
    for(std::size_t i=0;i<MaterialGpu::LIVE_TEXTURE_COUNT;++i){const Material::Slot slot=MaterialGpu::liveSlot(i);const auto& b=r->textures[Material::slotIndex(slot)];GL20.glUniform4f(tex_scale_offset_locations_[i],b.scale.x,b.scale.y,b.offset.x+b.turbulence.x,b.offset.y+b.turbulence.y);GL20.glUniform4f(tex_options_locations_[i],b.multiplier,static_cast<float>(channelCode(b.channel)),0.0f,0.0f);} return true;
}

bool GBufferGpu::createAttachments(std::string* error)
{
    if(framebuffer_==0) GL30.glGenFramebuffers(1,&framebuffer_);
    if(framebuffer_==0)
    {
        setError(error,"failed to allocate GPU GBuffer framebuffer");
        return false;
    }
    const bool ok=ensureColorTexture(&position_depth_,width_,height_,GBUFFER_POSITION_DEPTH_FORMAT)&&ensureColorTexture(&normal_roughness_,width_,height_,GBUFFER_NORMAL_ROUGHNESS_FORMAT)&&ensureColorTexture(&albedo_metallic_,width_,height_,GBUFFER_ALBEDO_METALLIC_FORMAT)&&ensureColorTexture(&emissive_opacity_,width_,height_,GBUFFER_EMISSIVE_FORMAT)&&ensureColorTexture(&specular_ior_,width_,height_,GBUFFER_SPECULAR_IOR_FORMAT)&&ensureColorTexture(&advanced_,width_,height_,GBUFFER_ADVANCED_FORMAT)&&ensureColorTexture(&ambient_transmission_,width_,height_,GBUFFER_TRANSMISSION_FORMAT)&&ensureColorTexture(&tangent_anisotropy_,width_,height_,GBUFFER_TANGENT_ANISOTROPY_FORMAT)&&ensureDepthTexture(&depth_,width_,height_);if(!ok){setError(error,"failed to allocate GPU GBuffer textures");destroyAttachments();return false;}
    GL30.glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_);GLuint textures[]={position_depth_,normal_roughness_,albedo_metallic_,emissive_opacity_,specular_ior_,advanced_,ambient_transmission_,tangent_anisotropy_};for(int i=0;i<8;++i)GL30.glFramebufferTexture2D(GL_FRAMEBUFFER,static_cast<GLenum>(GL_COLOR_ATTACHMENT0+i),GL_TEXTURE_2D,textures[i],0);GL30.glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth_,0);const GLenum outputs[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1,GL_COLOR_ATTACHMENT2,GL_COLOR_ATTACHMENT3,GL_COLOR_ATTACHMENT4,GL_COLOR_ATTACHMENT5,GL_COLOR_ATTACHMENT6,GL_COLOR_ATTACHMENT7};GL20.glDrawBuffers(8,outputs);const GLenum status=GL30.glCheckFramebufferStatus(GL_FRAMEBUFFER);GL30.glBindFramebuffer(GL_FRAMEBUFFER,0);glBindTexture(GL_TEXTURE_2D,0);if(status!=GL_FRAMEBUFFER_COMPLETE){char message[128];std::snprintf(message,sizeof(message),"GPU GBuffer framebuffer incomplete: 0x%04x",static_cast<unsigned>(status));setError(error,message);destroyAttachments();return false;}return true;
}

bool GBufferGpu::updateScene(const Ecs::World& world,std::string* error)
{
    if(!ready()){setError(error,"GPU GBuffer is not initialized or resized");return false;}cubes_.instances.clear();planes_.instances.clear();for(LoadedBatch& b:loaded_batches_)b.instances.clear();
    for(const Ecs::Entity entity:world.entities()){
        const auto* transform=world.getTransform(entity);const auto* mesh=world.getMesh(entity);const auto* renderable=world.getRenderable(entity);const auto* material=world.getMaterial(entity);if(!transform||!mesh||!renderable||!renderable->visible||!material)continue;
        const Material::Resource* resource=material->renderer_material!=Ecs::INVALID_ASSET_HANDLE?Material::get(material->renderer_material):nullptr;if(resource&&(resource->render_class==Material::RenderClass::Transparent||resource->render_class==Material::RenderClass::Transmissive))continue;
        InstanceGpu instance={};const Math::Vec3 position=toVec3(transform->position),rotation=toVec3(transform->rotation),scale=toVec3(transform->scale);const Math::Mat4 model=Math::transform(position,rotation,scale),normal=Math::multiply(Math::rotationEuler(rotation),Math::scaling({safeInverse(scale.x),safeInverse(scale.y),safeInverse(scale.z)}));std::memcpy(instance.model,model.value,sizeof(instance.model));std::memcpy(instance.normal_matrix,normal.value,sizeof(instance.normal_matrix));
        const Math::Vec3 albedo=resource?resource->base_color:toVec3(material->albedo),emissive=resource?resource->emissive:toVec3(material->emissive),ambient=resource?resource->ambient:toVec3(material->ambient),specular=resource?resource->specular:toVec3(material->specular),transmissionColor=resource?resource->transmission_color:toVec3(material->transmission_color);
        instance.albedo_metallic[0]=albedo.x;instance.albedo_metallic[1]=albedo.y;instance.albedo_metallic[2]=albedo.z;instance.albedo_metallic[3]=resource?resource->metallic:material->metallic;
        const float emissiveScale=resource?1.0f:material->emissive_strength;instance.emissive_roughness[0]=emissive.x*emissiveScale;instance.emissive_roughness[1]=emissive.y*emissiveScale;instance.emissive_roughness[2]=emissive.z*emissiveScale;instance.emissive_roughness[3]=resource?resource->roughness:material->roughness;
        instance.ambient_opacity[0]=ambient.x;instance.ambient_opacity[1]=ambient.y;instance.ambient_opacity[2]=ambient.z;instance.ambient_opacity[3]=resource?resource->opacity:material->opacity;
        const float specStrength=resource?resource->specular_strength:material->specular_strength;instance.specular_ior[0]=specular.x*specStrength;instance.specular_ior[1]=specular.y*specStrength;instance.specular_ior[2]=specular.z*specStrength;instance.specular_ior[3]=resource?resource->ior:material->ior;
        instance.advanced[0]=resource?resource->clearcoat:material->clearcoat;instance.advanced[1]=resource?resource->clearcoat_roughness:material->clearcoat_roughness;instance.advanced[2]=resource?resource->sheen:material->sheen;instance.advanced[3]=resource?resource->reflectivity:material->reflectivity;
        instance.transmission[0]=transmissionColor.x;instance.transmission[1]=transmissionColor.y;instance.transmission[2]=transmissionColor.z;instance.transmission[3]=resource?resource->transmission:material->transmission;instance.extra[0]=resource?resource->anisotropy:material->anisotropy;
        if(mesh->loaded_mesh!=Ecs::INVALID_ASSET_HANDLE){const Material::MaterialHandle mh=resource?material->renderer_material:Material::INVALID_MATERIAL;const Material::RenderClass rc=resource?resource->render_class:Material::RenderClass::Opaque;LoadedBatch* b=loadedBatch(mesh->loaded_mesh,mh,rc,error);if(!b)return false;b->instances.push_back(instance);}else if(mesh->mesh==Ecs::MeshType::Cube)cubes_.instances.push_back(instance);else planes_.instances.push_back(instance);
    }
    if(!uploadBatch(&cubes_,error)||!uploadBatch(&planes_,error)) return false;
    for(LoadedBatch& batch:loaded_batches_)
        if(batch.instance_buffer!=0 && !uploadBatch(&batch,error)) return false;
    if(error) error->clear();
    return true;
}

bool GBufferGpu::draw(const Math::Mat4& view,const Math::Mat4& projection,std::string* error)
{
    if(!ready()){setError(error,"GPU GBuffer is not initialized or resized");return false;}GL30.glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_);glViewport(0,0,width_,height_);const GLfloat zero[4]={0,0,0,0};for(int i=0;i<8;++i)GL30.glClearBufferfv(GL_COLOR,i,zero);glClearDepth(1.0);glClear(GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDisable(GL_BLEND);glEnable(GL_CULL_FACE);glCullFace(GL_BACK);GL20.glUseProgram(program_);GL20.glUniformMatrix4fv(view_location_,1,GL_FALSE,view.value);GL20.glUniformMatrix4fv(projection_location_,1,GL_FALSE,projection.value);
    auto drawProcedural=[&](const Batch& b)->bool{if(b.instances.empty())return true;if(!bindMaterial(Material::INVALID_MATERIAL,error))return false;GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,b.instance_buffer);GL30.glBindVertexArray(b.mesh.vao);GL31.glDrawElementsInstanced(GL_TRIANGLES,b.mesh.index_count,GL_UNSIGNED_INT,nullptr,static_cast<GLsizei>(b.instances.size()));return true;};
    if(!drawProcedural(cubes_)||!drawProcedural(planes_)) return false;
    for(const LoadedBatch& batch:loaded_batches_)
    {
        if(batch.instances.empty()) continue;
        if(!bindMaterial(batch.material,error)) return false;
        MeshGpu* mesh=loadedMeshGpu(batch.mesh_handle,error);
        if(!mesh) return false;
        GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,batch.instance_buffer);
        GL30.glBindVertexArray(mesh->vao);
        GL31.glDrawElementsInstanced(GL_TRIANGLES,mesh->index_count,GL_UNSIGNED_INT,nullptr,static_cast<GLsizei>(batch.instances.size()));
    }
    GL30.glBindVertexArray(0);GL20.glUseProgram(0);GL30.glBindFramebuffer(GL_FRAMEBUFFER,0);glDisable(GL_CULL_FACE);glDisable(GL_DEPTH_TEST);GL42.glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);if(error)error->clear();return true;
}

bool GBufferGpu::render(const Ecs::World& world,const Math::Mat4& view,const Math::Mat4& projection,std::string* error){return updateScene(world,error)&&draw(view,projection,error);}

void GBufferGpu::destroyAttachments(){deleteTexture(&position_depth_);deleteTexture(&normal_roughness_);deleteTexture(&albedo_metallic_);deleteTexture(&emissive_opacity_);deleteTexture(&specular_ior_);deleteTexture(&advanced_);deleteTexture(&ambient_transmission_);deleteTexture(&tangent_anisotropy_);deleteTexture(&depth_);if(framebuffer_!=0){GL30.glDeleteFramebuffers(1,&framebuffer_);framebuffer_=0;}}
void GBufferGpu::destroyMesh(MeshGpu* mesh){if(!mesh)return;if(mesh->index_buffer!=0)GL15.glDeleteBuffers(1,&mesh->index_buffer);if(mesh->vertex_buffer!=0)GL15.glDeleteBuffers(1,&mesh->vertex_buffer);if(mesh->vao!=0)GL30.glDeleteVertexArrays(1,&mesh->vao);*mesh={};}
void GBufferGpu::destroyBatch(Batch* b){if(!b)return;if(b->instance_buffer!=0)GL15.glDeleteBuffers(1,&b->instance_buffer);b->instance_buffer=0;b->instance_capacity=0;b->instances.clear();b->uploaded_instances.clear();destroyMesh(&b->mesh);}
void GBufferGpu::destroyBatch(LoadedBatch* b){if(!b)return;if(b->instance_buffer!=0)GL15.glDeleteBuffers(1,&b->instance_buffer);b->instance_buffer=0;b->instance_capacity=0;b->instances.clear();b->uploaded_instances.clear();}

void GBufferGpu::shutdown(){destroyAttachments();destroyBatch(&cubes_);destroyBatch(&planes_);for(LoadedBatch& b:loaded_batches_)destroyBatch(&b);loaded_batches_.clear();for(MeshGpu& mesh:loaded_meshes_)destroyMesh(&mesh);loaded_meshes_.clear();material_gpu_.shutdown();destroyProgram(&program_);view_location_=-1;projection_location_=-1;texture_mask_location_=-1;alpha_cutoff_location_=-1;masked_location_=-1;width_=0;height_=0;}

} }
