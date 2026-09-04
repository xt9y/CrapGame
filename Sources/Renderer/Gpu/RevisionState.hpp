#ifndef CRAPGAME_RENDERER_GPU_REVISIONSTATE_HPP
#define CRAPGAME_RENDERER_GPU_REVISIONSTATE_HPP

#include <cstdint>

namespace Renderer
{
namespace Gpu
{

struct RevisionState
{
    std::uint64_t geometry = 0u;
    std::uint64_t material = 0u;
    std::uint64_t lighting = 0u;
    std::uint64_t camera = 0u;
    std::uint64_t resolution = 0u;
    std::uint64_t mesh_registry = 0u;
    std::uint64_t material_registry = 0u;
};

inline bool sameFrameInputs (const RevisionState& a, const RevisionState& b)
{
    return a.geometry == b.geometry
        && a.material == b.material
        && a.lighting == b.lighting
        && a.camera == b.camera
        && a.resolution == b.resolution
        && a.mesh_registry == b.mesh_registry
        && a.material_registry == b.material_registry;
}

inline bool staticShadowValid (const RevisionState& cached, const RevisionState& current)
{
    return cached.geometry == current.geometry
        && cached.material == current.material
        && cached.lighting == current.lighting
        && cached.mesh_registry == current.mesh_registry
        && cached.material_registry == current.material_registry;
}

inline bool worldRadianceValid (const RevisionState& cached, const RevisionState& current)
{
    return cached.geometry == current.geometry
        && cached.material == current.material
        && cached.lighting == current.lighting
        && cached.mesh_registry == current.mesh_registry
        && cached.material_registry == current.material_registry;
}

inline void applyRevisionChanges (
            RevisionState *state,
            bool geometry_changed,
            bool material_changed,
            bool lighting_changed,
            bool camera_changed,
            std::uint64_t mesh_registry,
            std::uint64_t material_registry
    )
{
    if (!state) return;
    if (geometry_changed) ++state->geometry;
    if (material_changed) ++state->material;
    if (lighting_changed) ++state->lighting;
    if (camera_changed) ++state->camera;
    state->mesh_registry = mesh_registry;
    state->material_registry = material_registry;
}

} // namespace Gpu
} // namespace Renderer

#endif
