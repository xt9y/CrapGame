#ifndef CRAPGAME_RENDERER_LUMEN_SDF_TRACE_MATH_HPP
#define CRAPGAME_RENDERER_LUMEN_SDF_TRACE_MATH_HPP

#include "Renderer/Math/Math.hpp"

namespace Renderer
{
namespace Lumen
{

inline Math::Vec3 sdfTraceSamplePositionExact(
            const Math::Vec3& origin,
            const Math::Vec3& normalized_direction,
            float distance
    )
{
    const Math::Vec3 scaled = {
        normalized_direction.x * distance,
        normalized_direction.y * distance,
        normalized_direction.z * distance,
    };

    return {
        origin.x + scaled.x,
        origin.y + scaled.y,
        origin.z + scaled.z,
    };
}

} // namespace Lumen
} // namespace Renderer

#endif
