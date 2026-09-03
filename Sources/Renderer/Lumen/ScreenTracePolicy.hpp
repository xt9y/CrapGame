#ifndef CRAPGAME_RENDERER_LUMEN_SCREEN_TRACE_POLICY_HPP
#define CRAPGAME_RENDERER_LUMEN_SCREEN_TRACE_POLICY_HPP

#include "Renderer/Math/Math.hpp"

namespace Renderer
{
namespace Lumen
{

inline bool screenTraceWithinThicknessSquared(
            const Math::Vec3& delta,
            float thickness
    )
{
    if (thickness <= 0.0f)
    {
        return false;
    }

    const float distance_squared =
        delta.x * delta.x +
        delta.y * delta.y +
        delta.z * delta.z;

    return distance_squared <= thickness * thickness;
}

inline int screenTraceNormalizedPixelIndexExact(
            float normalized_coordinate,
            int extent
    )
{
    const int index = static_cast<int>(
            normalized_coordinate * static_cast<float>(extent)
        );

    return index >= extent ? extent - 1 : index;
}

} // namespace Lumen
} // namespace Renderer

#endif
