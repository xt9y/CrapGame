#ifndef CRAPGAME_RENDERER_LUMEN_TRILINEAR_INDEX_HPP
#define CRAPGAME_RENDERER_LUMEN_TRILINEAR_INDEX_HPP

#include <cstddef>

namespace Renderer
{
namespace Lumen
{

struct TrilinearIndices
{
    std::size_t x0y0z0,
                x1y0z0,
                x0y1z0,
                x1y1z0,
                x0y0z1,
                x1y0z1,
                x0y1z1,
                x1y1z1;
};

inline TrilinearIndices trilinearIndicesExact(
            int x0,
            int x1,
            int y0,
            int y1,
            int z0,
            int z1,
            int resolution,
            std::size_t resolution_squared
    )
{
    const std::size_t row0 = static_cast<std::size_t>(y0) *
                static_cast<std::size_t>(resolution),
            row1 = static_cast<std::size_t>(y1) *
                static_cast<std::size_t>(resolution),
            slice0 = static_cast<std::size_t>(z0) * resolution_squared,
            slice1 = static_cast<std::size_t>(z1) * resolution_squared,
            sx0 = static_cast<std::size_t>(x0),
            sx1 = static_cast<std::size_t>(x1);

    return {
        slice0 + row0 + sx0,
        slice0 + row0 + sx1,
        slice0 + row1 + sx0,
        slice0 + row1 + sx1,
        slice1 + row0 + sx0,
        slice1 + row0 + sx1,
        slice1 + row1 + sx0,
        slice1 + row1 + sx1,
    };
}

} // namespace Lumen
} // namespace Renderer

#endif
