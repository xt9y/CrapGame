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
struct InstanceData { mat4 model; mat4 normalMatrix; vec4 albedoMetallic; vec4 emissiveRoughness; };
layout(std430,binding=0) readonly buffer InstanceBuffer { InstanceData instances[]; };
uniform mat4 uView;
uniform mat4 uProjection;
out vec3 vWorldPosition;
out vec3 vWorldNormal;
out vec3 vAlbedo;
out vec3 vEmissive;
out float vMetallic;
out float vRoughness;
void main(){ InstanceData instance=instances[gl_InstanceID]; vec4 world=instance.model*vec4(aPosition,1.0); vWorldPosition=world.xyz; vWorldNormal=normalize(mat3(instance.normalMatrix)*aNormal); vAlbedo=instance.albedoMetallic.xyz; vMetallic=instance.albedoMetallic.w; vEmissive=instance.emissiveRoughness.xyz; vRoughness=instance.emissiveRoughness.w; gl_Position=uProjection*uView*world; }
)GLSL";

constexpr const char* GBUFFER_FRAGMENT_SHADER=R"GLSL(
#version 430 core
in vec3 vWorldPosition;
in vec3 vWorldNormal;
in vec3 vAlbedo;
in vec3 vEmissive;
in float vMetallic;
in float vRoughness;
layout(location=0) out vec4 oPositionDepth;
layout(location=1) out vec4 oNormalRoughness;
layout(location=2) out vec4 oAlbedoMetallic;
layout(location=3) out vec4 oEmissive;
void main(){ oPositionDepth=vec4(vWorldPosition,gl_FragCoord.z); oNormalRoughness=vec4(normalize(vWorldNormal),clamp(vRoughness,0.04,1.0)); oAlbedoMetallic=vec4(max(vAlbedo,vec3(0.0)),clamp(vMetallic,0.0,1.0)); oEmissive=vec4(max(vEmissive,vec3(0.0)),1.0); }
)GLSL";

Math::Vec3 toVec3(const Ecs::Vec3& v){ return {v.x,v.y,v.z}; }
float safeInverse(float v){ if(std::fabs(v)<0.00001f) return v<0.0f?-100000.0f:100000.0f; return 1.0f/v; }
GLint surfaceInternalFormat(SurfaceFormat f){ return f==SurfaceFormat::Rgba8?GL_RGBA8:GL_RGBA16F; }
GLenum surfacePixelType(SurfaceFormat f){ return f==SurfaceFormat::Rgba8?GL_UNSIGNED_BYTE:GL_FLOAT; }
void setError(std::string* error,const char* message){ if(error) *error=message?message:"GPU GBuffer error"; }

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
    if(view_location_<0 || projection_location_<0){ setError(error,"GPU GBuffer camera uniforms are unavailable"); shutdown(); return false; }
    if(!createMesh(Ecs::MeshType::Cube,&cubes_.mesh,error) || !createMesh(Ecs::MeshType::Plane,&planes_.mesh,error)){ shutdown(); return false; }
    GL15.glGenBuffers(1,&cubes_.instance_buffer);
    GL15.glGenBuffers(1,&planes_.instance_buffer);
    if(cubes_.instance_buffer==0 || planes_.instance_buffer==0){ setError(error,"failed to allocate GPU GBuffer instance buffers"); shutdown(); return false; }
    if(error) error->clear();
    return true;
}

bool GBufferGpu::resize(int width,int height,std::string* error)
{
    if(!resizeStorageRequired(width_,height_,framebuffer_!=0,width,height)) return true;
    width_=normalizedExtent(width); height_=normalizedExtent(height); return createAttachments(error);
}

bool GBufferGpu::createMesh(Ecs::MeshType type,MeshGpu* mesh,std::string* error){ return createMesh(Mesh::meshForType(type),mesh,error); }

bool GBufferGpu::createMesh(const Mesh::MeshData& source,MeshGpu* mesh,std::string* error)
{
    if(!mesh){ setError(error,"null GPU mesh destination"); return false; }
    if(source.vertices.empty() || source.indices.empty()){ setError(error,"cannot upload an empty renderer mesh"); return false; }
    GL30.glGenVertexArrays(1,&mesh->vao); GL15.glGenBuffers(1,&mesh->vertex_buffer); GL15.glGenBuffers(1,&mesh->index_buffer);
    if(mesh->vao==0 || mesh->vertex_buffer==0 || mesh->index_buffer==0){ setError(error,"failed to allocate persistent GPU mesh resources"); destroyMesh(mesh); return false; }
    GL30.glBindVertexArray(mesh->vao);
    GL15.glBindBuffer(GL_ARRAY_BUFFER,mesh->vertex_buffer);
    GL15.glBufferData(GL_ARRAY_BUFFER,static_cast<LWCGLsizeiptr>(source.vertices.size()*sizeof(Mesh::Vertex)),source.vertices.data(),GL_STATIC_DRAW);
    GL15.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,mesh->index_buffer);
    GL15.glBufferData(GL_ELEMENT_ARRAY_BUFFER,static_cast<LWCGLsizeiptr>(source.indices.size()*sizeof(std::uint32_t)),source.indices.data(),GL_STATIC_DRAW);
    GL20.glEnableVertexAttribArray(0); GL20.glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,static_cast<GLsizei>(sizeof(Mesh::Vertex)),reinterpret_cast<const void*>(offsetof(Mesh::Vertex,position)));
    GL20.glEnableVertexAttribArray(1); GL20.glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,static_cast<GLsizei>(sizeof(Mesh::Vertex)),reinterpret_cast<const void*>(offsetof(Mesh::Vertex,normal)));
    GL20.glEnableVertexAttribArray(2); GL20.glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,static_cast<GLsizei>(sizeof(Mesh::Vertex)),reinterpret_cast<const void*>(offsetof(Mesh::Vertex,uv)));
    GL30.glBindVertexArray(0); GL15.glBindBuffer(GL_ARRAY_BUFFER,0); GL15.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
    mesh->index_count=static_cast<GLsizei>(source.indices.size());
    return true;
}

GBufferGpu::Batch* GBufferGpu::loadedBatch(std::uint32_t handle,std::string* error)
{
    if(handle==Ecs::INVALID_ASSET_HANDLE){ setError(error,"invalid loaded mesh handle"); return nullptr; }
    if(handle>=loaded_batches_.size()) loaded_batches_.resize(static_cast<std::size_t>(handle)+1u);
    Batch& batch=loaded_batches_[handle];
    if(batch.mesh.vao==0)
    {
        const Mesh::MeshData* source=Mesh::loadedMesh(handle);
        if(!source){ setError(error,"loaded mesh handle is not registered"); return nullptr; }
        if(!createMesh(*source,&batch.mesh,error)) return nullptr;
        GL15.glGenBuffers(1,&batch.instance_buffer);
        if(batch.instance_buffer==0){ setError(error,"failed to allocate loaded-mesh instance buffer"); destroyMesh(&batch.mesh); return nullptr; }
    }
    return &batch;
}

bool GBufferGpu::uploadBatch(Batch* batch,std::string* error)
{
    if(!batch || batch->instance_buffer==0){ setError(error,"invalid GPU GBuffer batch"); return false; }
    if(batch->instances.empty()){ batch->uploaded_instances.clear(); return true; }
    const std::size_t required=batch->instances.size()*sizeof(InstanceGpu);
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER,batch->instance_buffer);
    const bool full=required>batch->instance_capacity || batch->instances.size()!=batch->uploaded_instances.size();
    if(required>batch->instance_capacity)
    {
        std::size_t capacity=std::max<std::size_t>(sizeof(InstanceGpu)*16u,batch->instance_capacity);
        while(capacity<required) capacity*=2u;
        GL15.glBufferData(GL_SHADER_STORAGE_BUFFER,static_cast<LWCGLsizeiptr>(capacity),nullptr,GL_DYNAMIC_DRAW);
        batch->instance_capacity=capacity;
    }
    if(full) GL15.glBufferSubData(GL_SHADER_STORAGE_BUFFER,0,static_cast<LWCGLsizeiptr>(required),batch->instances.data());
    else forEachDirtyRange(batch->instances,batch->uploaded_instances,[&](std::size_t first,std::size_t count){ GL15.glBufferSubData(GL_SHADER_STORAGE_BUFFER,static_cast<LWCGLintptr>(first*sizeof(InstanceGpu)),static_cast<LWCGLsizeiptr>(count*sizeof(InstanceGpu)),batch->instances.data()+first); });
    batch->uploaded_instances=batch->instances;
    return true;
}

bool GBufferGpu::createAttachments(std::string* error)
{
    if(framebuffer_==0) GL30.glGenFramebuffers(1,&framebuffer_);
    if(framebuffer_==0){ setError(error,"failed to allocate GPU GBuffer framebuffer"); return false; }
    const bool storage_ok=ensureColorTexture(&position_depth_,width_,height_,GBUFFER_POSITION_DEPTH_FORMAT) && ensureColorTexture(&normal_roughness_,width_,height_,GBUFFER_NORMAL_ROUGHNESS_FORMAT) && ensureColorTexture(&albedo_metallic_,width_,height_,GBUFFER_ALBEDO_METALLIC_FORMAT) && ensureColorTexture(&emissive_,width_,height_,GBUFFER_EMISSIVE_FORMAT) && ensureDepthTexture(&depth_,width_,height_);
    if(!storage_ok){ setError(error,"failed to allocate GPU GBuffer textures"); destroyAttachments(); return false; }
    GL30.glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_);
    GL30.glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,position_depth_,0);
    GL30.glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT1,GL_TEXTURE_2D,normal_roughness_,0);
    GL30.glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT2,GL_TEXTURE_2D,albedo_metallic_,0);
    GL30.glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT3,GL_TEXTURE_2D,emissive_,0);
    GL30.glFramebufferTexture2D(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depth_,0);
    const GLenum outputs[]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1,GL_COLOR_ATTACHMENT2,GL_COLOR_ATTACHMENT3};
    GL20.glDrawBuffers(4,outputs);
    const GLenum status=GL30.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    GL30.glBindFramebuffer(GL_FRAMEBUFFER,0); glBindTexture(GL_TEXTURE_2D,0);
    if(status!=GL_FRAMEBUFFER_COMPLETE){ char message[128]; std::snprintf(message,sizeof(message),"GPU GBuffer framebuffer incomplete: 0x%04x",static_cast<unsigned int>(status)); setError(error,message); destroyAttachments(); return false; }
    return true;
}

bool GBufferGpu::updateScene(const Ecs::World& world,std::string* error)
{
    if(!ready()){ setError(error,"GPU GBuffer is not initialized or resized"); return false; }
    cubes_.instances.clear(); planes_.instances.clear(); for(Batch& b:loaded_batches_) b.instances.clear();
    for(const Ecs::Entity entity:world.entities())
    {
        const auto* transform=world.getTransform(entity); const auto* mesh=world.getMesh(entity); const auto* renderable=world.getRenderable(entity); const auto* material=world.getMaterial(entity);
        if(!transform || !mesh || !renderable || !renderable->visible || !material) continue;
        InstanceGpu instance={}; const Math::Vec3 position=toVec3(transform->position), rotation=toVec3(transform->rotation), scale=toVec3(transform->scale);
        const Math::Mat4 model=Math::transform(position,rotation,scale); const Math::Mat4 normal=Math::multiply(Math::rotationEuler(rotation),Math::scaling({safeInverse(scale.x),safeInverse(scale.y),safeInverse(scale.z)}));
        std::memcpy(instance.model,model.value,sizeof(instance.model)); std::memcpy(instance.normal_matrix,normal.value,sizeof(instance.normal_matrix));
        instance.albedo_metallic[0]=material->albedo.x; instance.albedo_metallic[1]=material->albedo.y; instance.albedo_metallic[2]=material->albedo.z; instance.albedo_metallic[3]=material->metallic;
        instance.emissive_roughness[0]=material->emissive.x*material->emissive_strength; instance.emissive_roughness[1]=material->emissive.y*material->emissive_strength; instance.emissive_roughness[2]=material->emissive.z*material->emissive_strength; instance.emissive_roughness[3]=material->roughness;
        if(mesh->loaded_mesh!=Ecs::INVALID_ASSET_HANDLE){ Batch* b=loadedBatch(mesh->loaded_mesh,error); if(!b) return false; b->instances.push_back(instance); }
        else if(mesh->mesh==Ecs::MeshType::Cube) cubes_.instances.push_back(instance);
        else planes_.instances.push_back(instance);
    }
    if(!uploadBatch(&cubes_,error) || !uploadBatch(&planes_,error)) return false;
    for(Batch& b:loaded_batches_) if(b.instance_buffer!=0 && !uploadBatch(&b,error)) return false;
    if(error) error->clear();
    return true;
}

bool GBufferGpu::draw(const Math::Mat4& view,const Math::Mat4& projection,std::string* error)
{
    if(!ready()){ setError(error,"GPU GBuffer is not initialized or resized"); return false; }
    GL30.glBindFramebuffer(GL_FRAMEBUFFER,framebuffer_); glViewport(0,0,width_,height_);
    const GLfloat zero[4]={0,0,0,0}; GL30.glClearBufferfv(GL_COLOR,0,zero); GL30.glClearBufferfv(GL_COLOR,1,zero); GL30.glClearBufferfv(GL_COLOR,2,zero); GL30.glClearBufferfv(GL_COLOR,3,zero); glClearDepth(1.0); glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LESS); glDisable(GL_BLEND); glEnable(GL_CULL_FACE); glCullFace(GL_BACK);
    GL20.glUseProgram(program_); GL20.glUniformMatrix4fv(view_location_,1,GL_FALSE,view.value); GL20.glUniformMatrix4fv(projection_location_,1,GL_FALSE,projection.value);
    const auto draw_batch=[](const Batch& b){ if(b.instances.empty()) return; GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,0,b.instance_buffer); GL30.glBindVertexArray(b.mesh.vao); GL31.glDrawElementsInstanced(GL_TRIANGLES,b.mesh.index_count,GL_UNSIGNED_INT,nullptr,static_cast<GLsizei>(b.instances.size())); };
    draw_batch(cubes_); draw_batch(planes_); for(const Batch& b:loaded_batches_) draw_batch(b);
    GL30.glBindVertexArray(0); GL20.glUseProgram(0); GL30.glBindFramebuffer(GL_FRAMEBUFFER,0); glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST); GL42.glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);
    if(error) error->clear();
    return true;
}

bool GBufferGpu::render(const Ecs::World& world,const Math::Mat4& view,const Math::Mat4& projection,std::string* error){ return updateScene(world,error) && draw(view,projection,error); }

void GBufferGpu::destroyAttachments(){ deleteTexture(&position_depth_); deleteTexture(&normal_roughness_); deleteTexture(&albedo_metallic_); deleteTexture(&emissive_); deleteTexture(&depth_); if(framebuffer_!=0){ GL30.glDeleteFramebuffers(1,&framebuffer_); framebuffer_=0; } }
void GBufferGpu::destroyMesh(MeshGpu* mesh){ if(!mesh) return; if(mesh->index_buffer!=0) GL15.glDeleteBuffers(1,&mesh->index_buffer); if(mesh->vertex_buffer!=0) GL15.glDeleteBuffers(1,&mesh->vertex_buffer); if(mesh->vao!=0) GL30.glDeleteVertexArrays(1,&mesh->vao); *mesh={}; }
void GBufferGpu::destroyBatch(Batch* b){ if(!b) return; if(b->instance_buffer!=0) GL15.glDeleteBuffers(1,&b->instance_buffer); b->instance_buffer=0; b->instance_capacity=0; b->instances.clear(); b->uploaded_instances.clear(); destroyMesh(&b->mesh); }

void GBufferGpu::shutdown()
{
    destroyAttachments(); destroyBatch(&cubes_); destroyBatch(&planes_); for(Batch& b:loaded_batches_) destroyBatch(&b); loaded_batches_.clear(); destroyProgram(&program_); view_location_=-1; projection_location_=-1; width_=0; height_=0;
}

} }
