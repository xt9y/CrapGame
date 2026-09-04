#ifndef CRAPGAME_RENDERER_GPU_TRANSPARENTGPU_HPP
#define CRAPGAME_RENDERER_GPU_TRANSPARENTGPU_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Gpu/DirectLightingGpu.hpp"
#include "Renderer/Gpu/GBufferGpu.hpp"
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

class TransparentGpu
{
public:
    bool init(std::string *error=nullptr);
    bool resize(int width,int height,std::string *error=nullptr);
    bool updateScene(const Ecs::World& world,std::string *error=nullptr);
    bool render(const Ecs::World& world,const GBufferGpu& gbuffer,
                const DirectLightingGpu& direct,GLuint opaque_color,
                const Math::Mat4& view,const Math::Mat4& projection,
                const Math::Vec3& camera_position,
                std::string *error=nullptr);
    GLuint finalTexture() const { return last_output_; }
    bool hasTransparent() const { return !items_.empty(); }
    void shutdown();

private:
    struct MeshGpu
    {
        GLuint vao=0,vbo=0,ebo=0;
        GLsizei index_count=0;
        std::uint32_t handle=Ecs::INVALID_ASSET_HANDLE;
    };
    struct Item
    {
        std::uint32_t mesh=Ecs::INVALID_ASSET_HANDLE;
        Material::MaterialHandle material=Material::INVALID_MATERIAL;
        Math::Mat4 model=Math::identity();
        Math::Mat4 normal=Math::identity();
        Math::Vec3 center={0.0f,0.0f,0.0f};
        float depth=0.0f;
    };

    MeshGpu* meshGpu(std::uint32_t handle,std::string *error);
    bool bindMaterial(Material::MaterialHandle handle,std::string *error);
    bool allocateTarget(std::string *error);
    void destroyMesh(MeshGpu *mesh);
    void clearMeshes();

    GLuint program_=0,framebuffer_=0,copy_framebuffer_=0,
           final_color_=0,last_output_=0;
    GLint view_location_=-1,projection_location_=-1,model_location_=-1,
          normal_location_=-1,camera_location_=-1,viewport_location_=-1,
          light_count_location_=-1,texture_mask_location_=-1;
    GLint scalar0_location_=-1,scalar1_location_=-1,scalar2_location_=-1,
          color0_location_=-1,color1_location_=-1,
          ambient_location_=-1,transmission_color_location_=-1;
    GLint sampler_locations_[MaterialGpu::LIVE_TEXTURE_COUNT]={};
    GLint tex_scale_offset_locations_[MaterialGpu::LIVE_TEXTURE_COUNT]={};
    GLint tex_options_locations_[MaterialGpu::LIVE_TEXTURE_COUNT]={};
    MaterialGpu material_gpu_;
    std::vector<MeshGpu> meshes_;
    std::vector<Item> items_;
    std::uint64_t mesh_revision_=0u;
    int width_=0,height_=0;
};

} }
#endif
