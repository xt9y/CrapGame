#ifndef CRAPGAME_RENDERER_LUMEN_TRACE_DIRECTION_HPP
#define CRAPGAME_RENDERER_LUMEN_TRACE_DIRECTION_HPP

#include "Renderer/Math/Math.hpp"

#include <cmath>

namespace Renderer
{
namespace Lumen
{

inline Math::Vec3 normalizedTraceDirection(const Math::Vec3& direction)
{
    constexpr float EPSILON = 0.000001f;

    const float length_squared =
        direction.x * direction.x +
        direction.y * direction.y +
        direction.z * direction.z;
    const float value_length = std::sqrt(length_squared);

    if (value_length <= EPSILON)
    {
        return {0.0f, 0.0f, 0.0f};
    }

    const float inverse = 1.0f / value_length;
    return {
        direction.x * inverse,
        direction.y * inverse,
        direction.z * inverse,
    };
}

} // namespace Lumen
} // namespace Renderer

#endif
