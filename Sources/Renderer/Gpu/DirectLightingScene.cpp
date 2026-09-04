#include "DirectLightingGpu.hpp"
#include "Renderer/Gpu/DirtyRanges.hpp"
#include "Renderer/Gpu/SurfaceFormats.hpp"
#include "Renderer/Gpu/TransparentGpu.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>

namespace Renderer { namespace Gpu { namespace {
GLenum directImageFormat(SurfaceFormat format){return format==SurfaceFormat::Rgba8?GL_RGBA8:GL_RGBA16F;}
void sceneError(std::string *error,const char *message){if(error)*error=message?message:"GPU direct-light scene error";}
} // namespace

const BvhBenchConfig& DirectLightingGpu::benchConfig(){if(!bench_config_initialized_){bench_config_=bvhBenchConfig();bench_config_initialized_=true;}return bench_config_;}

void DirectLightingGpu::appendStressPrimitives(std::size_t count)
{
    if(count==0u) return;
    std::size_t side=1u;
    while(side*side<count) ++side;
    const float half=static_cast<float>(side-1u)*0.5f;
    for(std::size_t index=0;index<count;++index){const std::size_t gx=index%side,gz=index/side;Ecs::TransformComponent tr={};tr.position={(static_cast<float>(gx)-half)*1.20f,0.55f+static_cast<float>(index%3u)*0.18f,(static_cast<float>(gz)-half)*1.20f-3.0f};tr.rotation={static_cast<float>((index*13u)%31u),static_cast<float>((index*29u)%360u),0};tr.scale={0.32f,0.32f,0.32f};PrimitiveGpu p={};p.position_type[0]=tr.position.x;p.position_type[1]=tr.position.y;p.position_type[2]=tr.position.z;p.position_type[3]=0;p.rotation[0]=tr.rotation.x;p.rotation[1]=tr.rotation.y;p.rotation[2]=tr.rotation.z;p.scale[0]=tr.scale.x;p.scale[1]=tr.scale.y;p.scale[2]=tr.scale.z;p.albedo_metallic[0]=0.35f+static_cast<float>(index%5u)*0.07f;p.albedo_metallic[1]=0.42f;p.albedo_metallic[2]=0.55f;p.albedo_metallic[3]=index%4u==0u?0.55f:0;p.emissive_roughness[3]=0.45f;const std::uint32_t pi=static_cast<std::uint32_t>(primitives_.size());primitives_.push_back(p);primitive_bounds_.push_back(primitiveBounds(tr,Ecs::MeshType::Cube,pi));}
}

bool DirectLightingGpu::updateScene(const Ecs::World& world,std::string *error)
{
    if(program_==0||light_buffer_==0||primitive_buffer_==0){sceneError(error,"GPU direct-light scene buffers are not initialized");return false;}
    scene_world_=&world;scene_revision_=world.changeRevision();
    if(!triangle_scene_.update(world,error))return false;
    if(transparent_&&!transparent_->updateScene(world,error))return false;
    lights_.clear();primitives_.clear();primitive_bounds_.clear();
    for(const Ecs::Entity entity:world.entities())
    {
        const auto* tr=world.getTransform(entity);const auto* light=world.getLight(entity);
        if(tr&&light&&light->intensity>0){LightGpu g={};const Math::Vec3 rot={tr->rotation.x,tr->rotation.y,tr->rotation.z};const Math::Vec3 f=Math::normalize(Math::transformDirection(Math::rotationEuler(rot),{0,0,-1}));g.position_type[0]=tr->position.x;g.position_type[1]=tr->position.y;g.position_type[2]=tr->position.z;g.position_type[3]=light->type==Ecs::LightType::Directional?0:(light->type==Ecs::LightType::Point?1:2);g.direction_range[0]=f.x;g.direction_range[1]=f.y;g.direction_range[2]=f.z;g.direction_range[3]=light->range;g.color_intensity[0]=light->color.x;g.color_intensity[1]=light->color.y;g.color_intensity[2]=light->color.z;g.color_intensity[3]=light->intensity;g.cone_shadow[0]=std::cos(Math::radians(light->inner_cone));g.cone_shadow[1]=std::cos(Math::radians(light->outer_cone));g.cone_shadow[2]=light->casts_shadows?1:0;g.cone_shadow[3]=light->indirect_intensity;lights_.push_back(g);}
        const auto* mesh=world.getMesh(entity);const auto* renderable=world.getRenderable(entity);const auto* material=world.getMaterial(entity);if(!tr||!mesh||!renderable||!renderable->visible||!material||mesh->loaded_mesh!=Ecs::INVALID_ASSET_HANDLE)continue;
        PrimitiveGpu p={};p.position_type[0]=tr->position.x;p.position_type[1]=tr->position.y;p.position_type[2]=tr->position.z;p.position_type[3]=mesh->mesh==Ecs::MeshType::Cube?0:1;p.rotation[0]=tr->rotation.x;p.rotation[1]=tr->rotation.y;p.rotation[2]=tr->rotation.z;p.scale[0]=tr->scale.x;p.scale[1]=tr->scale.y;p.scale[2]=tr->scale.z;p.scale[3]=1;p.albedo_metallic[0]=material->albedo.x;p.albedo_metallic[1]=material->albedo.y;p.albedo_metallic[2]=material->albedo.z;p.albedo_metallic[3]=material->metallic;p.emissive_roughness[0]=material->emissive.x*material->emissive_strength;p.emissive_roughness[1]=material->emissive.y*material->emissive_strength;p.emissive_roughness[2]=material->emissive.z*material->emissive_strength;p.emissive_roughness[3]=material->roughness;const std::uint32_t pi=static_cast<std::uint32_t>(primitives_.size());primitives_.push_back(p);primitive_bounds_.push_back(primitiveBounds(*tr,mesh->mesh,pi));
    }
    const std::size_t scene_count=primitives_.size();const BvhBenchConfig& cfg=benchConfig();appendStressPrimitives(cfg.stress_primitives);use_bvh_=shouldUseBvh(cfg.mode,primitives_.size(),BVH_THRESHOLD);
    if(!bench_reported_&&(cfg.stress_primitives>0u||cfg.mode!=BvhMode::Auto)){std::fprintf(stderr,"GPU BVH benchmark: scene %zu + stress %zu = %zu primitives, mode %s, traversal %s\n",scene_count,cfg.stress_primitives,primitives_.size(),bvhModeName(cfg.mode),use_bvh_?"bvh":"linear");bench_reported_=true;}
    if(!uploadChangedRecords(light_buffer_,&light_capacity_,lights_,&uploaded_lights_,error)||!uploadChangedRecords(primitive_buffer_,&primitive_capacity_,primitives_,&uploaded_primitives_,error))return false;
    if(use_bvh_){if(!ensureBvhBuffer(error))return false;const bool match=!bvh_nodes_.empty()&&bvh_primitive_count_==primitives_.size();bool refit=match&&refitBvh(&bvh_nodes_,primitive_bounds_);if(!refit){BvhBuild build=buildBvh(primitive_bounds_,BVH_LEAF_SIZE);bvh_nodes_=std::move(build.nodes);bvh_primitive_count_=primitives_.size();}if(!uploadChangedRecords(bvh_node_buffer_,&bvh_node_capacity_,bvh_nodes_,&uploaded_bvh_nodes_,error))return false;}else{bvh_nodes_.clear();bvh_primitive_count_=0;}
    if(error) error->clear();
    return true;
}

bool DirectLightingGpu::dispatch(const GBufferGpu& g,const Math::Vec3& camera,std::string *error)
{
    if(!ready()||!g.ready()||g.width()!=width_||g.height()!=height_){sceneError(error,"GPU direct lighting resources are not ready for this GBuffer");return false;}
    if(!bindImportedScene(triangle_scene_,error))return false;
    GLModern.glActiveTexture(GL_TEXTURE0+0);glBindTexture(GL_TEXTURE_2D,g.specularIorTexture());GLModern.glActiveTexture(GL_TEXTURE0+1);glBindTexture(GL_TEXTURE_2D,g.advancedMaterialTexture());GLModern.glActiveTexture(GL_TEXTURE0+2);glBindTexture(GL_TEXTURE_2D,g.transmissionTexture());GLModern.glActiveTexture(GL_TEXTURE0+3);glBindTexture(GL_TEXTURE_2D,g.tangentAnisotropyTexture());GLModern.glActiveTexture(GL_TEXTURE0+4);glBindTexture(GL_TEXTURE_2D_ARRAY,triangle_scene_.colorAtlas());GLModern.glActiveTexture(GL_TEXTURE0+5);glBindTexture(GL_TEXTURE_2D_ARRAY,triangle_scene_.dataAtlas());GLModern.glActiveTexture(GL_TEXTURE0);
    GL20.glUseProgram(program_);GL20.glUniform3f(camera_location_,camera.x,camera.y,camera.z);GL20.glUniform1i(light_count_location_,static_cast<GLint>(lights_.size()));GL20.glUniform1i(primitive_count_location_,static_cast<GLint>(primitives_.size()));GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,5,light_buffer_);GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,6,primitive_buffer_);GL42.glBindImageTexture(0,g.positionDepthTexture(),0,GL_FALSE,0,GL_READ_ONLY,directImageFormat(GBUFFER_POSITION_DEPTH_FORMAT));GL42.glBindImageTexture(1,g.normalRoughnessTexture(),0,GL_FALSE,0,GL_READ_ONLY,directImageFormat(GBUFFER_NORMAL_ROUGHNESS_FORMAT));GL42.glBindImageTexture(2,g.albedoMetallicTexture(),0,GL_FALSE,0,GL_READ_ONLY,directImageFormat(GBUFFER_ALBEDO_METALLIC_FORMAT));GL42.glBindImageTexture(3,g.emissiveTexture(),0,GL_FALSE,0,GL_READ_ONLY,directImageFormat(GBUFFER_EMISSIVE_FORMAT));GL42.glBindImageTexture(4,direct_color_,0,GL_FALSE,0,GL_WRITE_ONLY,directImageFormat(DIRECT_COLOR_FORMAT));GL43.glDispatchCompute(static_cast<GLuint>((width_+7)/8),static_cast<GLuint>((height_+7)/8),1);GL42.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_FETCH_BARRIER_BIT);GL20.glUseProgram(0);if(error)error->clear();return true;
}

bool DirectLightingGpu::ensureBvhBuffer(std::string *error){if(bvh_node_buffer_==0)GL15.glGenBuffers(1,&bvh_node_buffer_);if(bvh_node_buffer_==0){sceneError(error,"failed to allocate shared GPU BVH buffer");return false;}return true;}

template <typename T> bool DirectLightingGpu::uploadChangedRecords(GLuint buffer,std::size_t *capacity,const std::vector<T>& current,std::vector<T> *uploaded,std::string *error){if(!capacity||!uploaded){sceneError(error,"invalid GPU dirty-range destination");return false;}if(current.empty()){if(*capacity==0&&!uploadBuffer(buffer,capacity,nullptr,0,error))return false;uploaded->clear();return true;}const std::size_t required=current.size()*sizeof(T);if(*capacity==0||required>*capacity||current.size()!=uploaded->size()){if(!uploadBuffer(buffer,capacity,current.data(),required,error))return false;*uploaded=current;return true;}GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER,buffer);forEachDirtyRange(current,*uploaded,[&](std::size_t first,std::size_t count){GL15.glBufferSubData(GL_SHADER_STORAGE_BUFFER,static_cast<LWCGLintptr>(first*sizeof(T)),static_cast<LWCGLsizeiptr>(count*sizeof(T)),current.data()+first);});*uploaded=current;return true;}

} }
