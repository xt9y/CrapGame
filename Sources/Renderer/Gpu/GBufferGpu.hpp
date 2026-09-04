#ifndef CRAPGAME_RENDERER_GPU_GBUFFERGPU_HPP
#define CRAPGAME_RENDERER_GPU_GBUFFERGPU_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Gpu/MaterialGpu.hpp"
#include "Renderer/Material/Material.hpp"
#include "Renderer/Math/Math.hpp"
#include "Renderer/Mesh/Mesh.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Renderer { namespace Gpu {

class GBufferGpu
{
public:
    bool init(std::string* error=nullptr);
    bool resize(int width,int height,std::string* error=nullptr);
    bool updateScene(const Ecs::World& world,std::string* error=nullptr);

    bool prewarm(const Ecs::World& world,std::string* error=nullptr)
    {
        if(!updateScene(world,error)) return false;

        for(const Ecs::Entity entity:world.entities())
        {
            const Ecs::RenderableComponent* renderable=world.getRenderable(entity);
            const Ecs::MaterialComponent* material=world.getMaterial(entity);
            if(!renderable||!renderable->visible||!material
                    || material->renderer_material==Ecs::INVALID_ASSET_HANDLE)
            {
                continue;
            }

            if(!material_gpu_.ensure(material->renderer_material,error))
                return false;
        }

        if(error) error->clear();
        return true;
    }

    bool draw(const Math::Mat4& view,const Math::Mat4& projection,std::string* error=nullptr);
    bool render(const Ecs::World& world,const Math::Mat4& view,const Math::Mat4& projection,std::string* error=nullptr);
    void shutdown();

    bool ready() const { return program_!=0 && framebuffer_!=0; }
    int width() const { return width_; }
    int height() const { return height_; }
    GLuint positionDepthTexture() const { return position_depth_; }
    GLuint normalRoughnessTexture() const { return normal_roughness_; }
    GLuint albedoMetallicTexture() const { return albedo_metallic_; }
    GLuint emissiveTexture() const { return emissive_opacity_; }
    GLuint specularIorTexture() const { return specular_ior_; }
    GLuint advancedMaterialTexture() const { return advanced_; }
    GLuint transmissionTexture() const { return ambient_transmission_; }
    GLuint tangentAnisotropyTexture() const { return tangent_anisotropy_; }
    GLuint depthTexture() const { return depth_; }

private:
    struct MeshGpu { GLuint vao=0, vertex_buffer=0, index_buffer=0; GLsizei index_count=0; };
    struct InstanceGpu
    {
        float model[16];
        float normal_matrix[16];
        float albedo_metallic[4];
        float emissive_roughness[4];
        float ambient_opacity[4];
        float specular_ior[4];
        float advanced[4];
        float transmission[4];
        float extra[4];
    };
    struct Batch
    {
        MeshGpu mesh;
        GLuint instance_buffer=0;
        std::size_t instance_capacity=0;
        std::vector<InstanceGpu> instances;
        std::vector<InstanceGpu> uploaded_instances;
    };
    struct LoadedBatch
    {
        std::uint32_t mesh_handle=Ecs::INVALID_ASSET_HANDLE;
        Material::MaterialHandle material=Material::INVALID_MATERIAL;
        Material::RenderClass render_class=Material::RenderClass::Opaque;
        GLuint instance_buffer=0;
        std::size_t instance_capacity=0;
        std::vector<InstanceGpu> instances;
        std::vector<InstanceGpu> uploaded_instances;
    };

    bool createMesh(Ecs::MeshType type,MeshGpu* mesh,std::string* error);
    bool createMesh(const Mesh::MeshData& source,MeshGpu* mesh,std::string* error);
    MeshGpu* loadedMeshGpu(std::uint32_t handle,std::string* error);
    LoadedBatch* loadedBatch(std::uint32_t handle,Material::MaterialHandle material,Material::RenderClass render_class,std::string* error);
    bool uploadBatch(Batch* batch,std::string* error);
    bool uploadBatch(LoadedBatch* batch,std::string* error);
    bool bindMaterial(Material::MaterialHandle material,std::string* error);
    bool createAttachments(std::string* error);
    void destroyAttachments();
    void destroyMesh(MeshGpu* mesh);
    void destroyBatch(Batch* batch);
    void destroyBatch(LoadedBatch* batch);

    GLuint program_=0, framebuffer_=0;
    GLuint position_depth_=0, normal_roughness_=0, albedo_metallic_=0,
           emissive_opacity_=0, specular_ior_=0, advanced_=0,
           ambient_transmission_=0, tangent_anisotropy_=0, depth_=0;
    GLint view_location_=-1, projection_location_=-1;
    GLint texture_mask_location_=-1, alpha_cutoff_location_=-1, masked_location_=-1;
    GLint sampler_locations_[MaterialGpu::LIVE_TEXTURE_COUNT] = {};
    GLint tex_scale_offset_locations_[MaterialGpu::LIVE_TEXTURE_COUNT] = {};
    GLint tex_options_locations_[MaterialGpu::LIVE_TEXTURE_COUNT] = {};
    MaterialGpu material_gpu_;
    Batch cubes_, planes_;
    std::vector<MeshGpu> loaded_meshes_;
    std::vector<LoadedBatch> loaded_batches_;
    int width_=0, height_=0;
};

} }
#endif