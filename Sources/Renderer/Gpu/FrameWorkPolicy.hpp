#ifndef CRAPGAME_RENDERER_GPU_FRAMEWORKPOLICY_HPP
#define CRAPGAME_RENDERER_GPU_FRAMEWORKPOLICY_HPP

#include "Renderer/Gpu/RevisionState.hpp"

#include <cstdint>

namespace Renderer
{
namespace Gpu
{

struct FrameWork
{
    bool geometry=false;
    bool shadow=false;
    bool reprojection=false;
    bool dirty_tiles=false;
    bool static_diffuse=false;
    bool view_specular=false;
    bool lumen_trace=false;
    bool composite=false;
    bool transparent=false;
    bool present=true;
};

inline bool frameWorldGeometryChanged(const RevisionState& previous,const RevisionState& current)
{
    return previous.geometry!=current.geometry
        || previous.mesh_registry!=current.mesh_registry;
}

inline bool frameResolutionChanged(const RevisionState& previous,const RevisionState& current)
{
    return previous.resolution!=current.resolution;
}

inline bool frameMaterialChanged(const RevisionState& previous,const RevisionState& current)
{
    return previous.material!=current.material
        || previous.material_registry!=current.material_registry;
}

inline bool frameLightingChanged(const RevisionState& previous,const RevisionState& current)
{
    return previous.lighting!=current.lighting;
}

inline bool frameCameraChanged(const RevisionState& previous,const RevisionState& current)
{
    return previous.camera!=current.camera
        || previous.resolution!=current.resolution;
}

inline FrameWork decideFrameWork(const RevisionState& previous,
                                 const RevisionState& current,
                                 bool converged,
                                 bool camera_moving,
                                 bool transparent_dynamic)
{
    const bool geometry_changed=frameWorldGeometryChanged(previous,current);
    const bool resolution_changed=frameResolutionChanged(previous,current);
    const bool material_changed=frameMaterialChanged(previous,current);
    const bool lighting_changed=frameLightingChanged(previous,current);
    const bool camera_changed=frameCameraChanged(previous,current)||camera_moving;
    const bool world_changed=geometry_changed||material_changed||lighting_changed;
    const bool any_changed=world_changed||resolution_changed||camera_changed;

    FrameWork work;
    if(converged&&!any_changed&&!transparent_dynamic)
        return work;

    work.geometry=geometry_changed||material_changed||resolution_changed||camera_changed;
    work.shadow=world_changed;
    work.reprojection=camera_changed&&!world_changed;
    work.dirty_tiles=work.reprojection;
    work.static_diffuse=geometry_changed||material_changed||lighting_changed||camera_changed;
    work.view_specular=work.static_diffuse;
    work.lumen_trace=world_changed||camera_changed||!converged;
    work.composite=work.static_diffuse||work.view_specular||work.lumen_trace;
    work.transparent=transparent_dynamic||work.composite||camera_changed;
    return work;
}

constexpr std::uint64_t DEFAULT_MOVING_SECONDARY_INTERVAL_NS=66666667ull;

inline std::uint64_t movingSecondaryIntervalNanoseconds(std::uint32_t configured_hz)
{
    if(configured_hz==0u)return DEFAULT_MOVING_SECONDARY_INTERVAL_NS;
    return 1000000000ull/static_cast<std::uint64_t>(configured_hz);
}

inline bool movingSecondaryRefreshDue(std::uint64_t now_ns,
                                      std::uint64_t last_refresh_ns,
                                      std::uint32_t configured_hz,
                                      bool invalidated)
{
    if(invalidated||last_refresh_ns==0u||now_ns<last_refresh_ns)return true;
    return now_ns-last_refresh_ns>=movingSecondaryIntervalNanoseconds(configured_hz);
}

} // namespace Gpu
} // namespace Renderer

#endif
