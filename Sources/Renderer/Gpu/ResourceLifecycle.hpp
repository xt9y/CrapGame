#ifndef CRAPGAME_RENDERER_GPU_RESOURCELIFECYCLE_HPP
#define CRAPGAME_RENDERER_GPU_RESOURCELIFECYCLE_HPP

namespace Renderer
{
namespace Gpu
{

constexpr int normalizedExtent (int value)
{
    return value > 0 ? value : 1;
}

constexpr bool resizeStorageRequired (
            int current_width,
            int current_height,
            bool resources_ready,
            int requested_width,
            int requested_height
    )
{
    const int width = normalizedExtent(requested_width);
    const int height = normalizedExtent(requested_height);
    return !resources_ready
        || current_width != width
        || current_height != height;
}

} // namespace Gpu
} // namespace Renderer

#endif
