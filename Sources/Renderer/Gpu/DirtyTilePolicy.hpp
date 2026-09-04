#ifndef CRAPGAME_RENDERER_GPU_DIRTYTILEPOLICY_HPP
#define CRAPGAME_RENDERER_GPU_DIRTYTILEPOLICY_HPP

#include <algorithm>
#include <cstdint>

namespace Renderer
{
namespace Gpu
{

struct DirtyTilePolicy
{
    static constexpr int TILE_SIZE = 8;

    static constexpr int tileCount (int extent)
    {
        return (std::max(extent, 1) + TILE_SIZE - 1) / TILE_SIZE;
    }

    static bool tileDirty (
                const std::uint8_t *valid_mask,
                int width,
                int height,
                int tile_x,
                int tile_y
        )
    {
        if (!valid_mask || width <= 0 || height <= 0 || tile_x < 0 || tile_y < 0)
            return true;

        const int x0 = tile_x * TILE_SIZE;
        const int y0 = tile_y * TILE_SIZE;
        if (x0 >= width || y0 >= height) return false;

        const int x1 = std::min(x0 + TILE_SIZE, width);
        const int y1 = std::min(y0 + TILE_SIZE, height);
        for (int y = y0; y < y1; ++y)
        {
            for (int x = x0; x < x1; ++x)
            {
                if (valid_mask[y * width + x] == 0u) return true;
            }
        }
        return false;
    }
};

} // namespace Gpu
} // namespace Renderer

#endif
