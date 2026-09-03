#ifndef CRAPGAME_RENDERER_LUMEN_SDF_BROADPHASE_HPP
#define CRAPGAME_RENDERER_LUMEN_SDF_BROADPHASE_HPP

#include "Renderer/Math/Math.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer
{
namespace Lumen
{

struct SdfWorldBounds
{
    Math::Vec3 minimum,
               maximum;
};

/* Exact lower bound from a point to an axis-aligned world-space box. If this
 * is already farther than the nearest SDF sample, that instance cannot win
 * the distance query and its inverse-transform/3D texture sample is skipped. */
inline float sdfBoundsDistance (
            const SdfWorldBounds& bounds,
            const Math::Vec3& position
    )
{
    const float dx = std::max(
                std::max(bounds.minimum.x - position.x, 0.0f),
                position.x - bounds.maximum.x
            ),
            dy = std::max(
                std::max(bounds.minimum.y - position.y, 0.0f),
                position.y - bounds.maximum.y
            ),
            dz = std::max(
                std::max(bounds.minimum.z - position.z, 0.0f),
                position.z - bounds.maximum.z
            );

    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

} // namespace Lumen
} // namespace Renderer

#endif
