#ifndef CRAPGAME_RENDERER_GPU_STATICSHADOWCACHEGPU_HPP
#define CRAPGAME_RENDERER_GPU_STATICSHADOWCACHEGPU_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Gpu/MaterialGpu.hpp"
#include "Renderer/Gpu/RevisionState.hpp"
#include "Renderer/Gpu/StaticShadowPolicy.hpp"
#include "Renderer/Math/Math.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cstdint>
#include <string>
#include <vector>

namespace Renderer
{
namespace Gpu
{

class TriangleScene;

class StaticShadowCacheGpu
{
public:
    static constexpr int SIZE = StaticShadowPolicy::SIZE;

    bool init (std::string *error = nullptr);
    bool ensure (
        const Ecs::World& world,
        const TriangleScene& triangles,
        const RevisionState& revisions,
        std::string *error = nullptr
    );
    bool bind (
        GLuint direct_program,
        int light_index,
        std::string *error = nullptr
    );
    bool validFor (const RevisionState& revisions) const;
    void shutdown ();

    bool enabled () const { return enabled_; }
    GLuint depthTexture () const { return depth_texture_; }
    const Math::Mat4& lightViewProjection () const { return light_view_projection_; }
    Ecs::Entity lightEntity () const { return light_entity_; }

private:
    struct MeshGpu
    {
        std::uint32_t handle = Ecs::INVALID_ASSET_HANDLE;
        GLuint vao = 0;
        GLuint vertex_buffer = 0;
        GLuint index_buffer = 0;
        GLsizei index_count = 0;
    };

    bool createStorage (std::string *error);
    bool createMesh (
        std::uint32_t handle,
        MeshGpu *mesh,
        std::string *error
    );
    MeshGpu *meshFor (std::uint32_t handle, std::string *error);
    bool buildLightMatrix (
        const Ecs::World& world,
        const Ecs::TransformComponent& light_transform,
        std::string *error
    );
    bool drawScene (const Ecs::World& world, std::string *error);
    bool bindMaterial (
        const Ecs::MaterialComponent& material,
        std::string *error
    );
    void destroyMesh (MeshGpu *mesh);
    void clearMeshes ();
    void disable ();

    GLuint program_ = 0;
    GLuint framebuffer_ = 0;
    GLuint depth_texture_ = 0;
    GLint model_location_ = -1;
    GLint light_view_projection_location_ = -1;
    GLint masked_location_ = -1;
    GLint has_opacity_texture_location_ = -1;
    GLint opacity_location_ = -1;
    GLint alpha_cutoff_location_ = -1;
    GLint opacity_scale_offset_location_ = -1;
    GLint opacity_channel_location_ = -1;
    GLint opacity_sampler_location_ = -1;
    GLuint direct_program_ = 0;
    GLint direct_enabled_location_ = -1;
    GLint direct_light_index_location_ = -1;
    GLint direct_matrix_location_ = -1;

    MaterialGpu material_gpu_;
    std::vector<MeshGpu> meshes_;
    RevisionState revisions_ = {};
    Math::Mat4 light_view_projection_ = Math::identity();
    Ecs::Entity light_entity_ = Ecs::INVALID_ENTITY;
    std::uint64_t mesh_revision_ = 0u;
    bool enabled_ = false;
    bool valid_ = false;
};

} // namespace Gpu
} // namespace Renderer

#endif
