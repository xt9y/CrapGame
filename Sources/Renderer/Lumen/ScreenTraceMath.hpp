#ifndef CRAPGAME_RENDERER_LUMEN_SCREEN_TRACE_MATH_HPP
#define CRAPGAME_RENDERER_LUMEN_SCREEN_TRACE_MATH_HPP

#include "Renderer/Math/Math.hpp"

namespace Renderer
{
namespace Lumen
{

inline Math::Vec3 screenTraceSamplePosition(
            const Math::Vec3& origin,
            const Math::Vec3& normalized_direction,
            float distance
    )
{
    return {
        origin.x + normalized_direction.x * distance,
        origin.y + normalized_direction.y * distance,
        origin.z + normalized_direction.z * distance,
    };
}

inline Math::Vec4 screenTraceTransformPoint(
            const Math::Mat4& matrix,
            const Math::Vec3& value
    )
{
    return {
        matrix.value[0] * value.x + matrix.value[4] * value.y + matrix.value[8]  * value.z + matrix.value[12],
        matrix.value[1] * value.x + matrix.value[5] * value.y + matrix.value[9]  * value.z + matrix.value[13],
        matrix.value[2] * value.x + matrix.value[6] * value.y + matrix.value[10] * value.z + matrix.value[14],
        matrix.value[3] * value.x + matrix.value[7] * value.y + matrix.value[11] * value.z + matrix.value[15],
    };
}

inline Math::Vec4 screenTraceTransform(
            const Math::Mat4& matrix,
            const Math::Vec4& value
    )
{
    return {
        matrix.value[0] * value.x + matrix.value[4] * value.y + matrix.value[8]  * value.z + matrix.value[12] * value.w,
        matrix.value[1] * value.x + matrix.value[5] * value.y + matrix.value[9]  * value.z + matrix.value[13] * value.w,
        matrix.value[2] * value.x + matrix.value[6] * value.y + matrix.value[10] * value.z + matrix.value[14] * value.w,
        matrix.value[3] * value.x + matrix.value[7] * value.y + matrix.value[11] * value.z + matrix.value[15] * value.w,
    };
}

} // namespace Lumen
} // namespace Renderer

#endif
