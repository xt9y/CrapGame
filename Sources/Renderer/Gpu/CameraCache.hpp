#ifndef CRAPGAME_RENDERER_GPU_CAMERACACHE_HPP
#define CRAPGAME_RENDERER_GPU_CAMERACACHE_HPP

namespace Renderer
{
namespace Gpu
{

inline bool shouldUpdateCameraMatrices (
            bool cache_valid,
            bool camera_changed,
            bool viewport_changed
    )
{
    return !cache_valid || camera_changed || viewport_changed;
}

} // namespace Gpu
} // namespace Renderer

#endif
