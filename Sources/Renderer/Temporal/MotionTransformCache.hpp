#ifndef CRAPGAME_RENDERER_TEMPORAL_MOTION_TRANSFORM_CACHE_HPP
#define CRAPGAME_RENDERER_TEMPORAL_MOTION_TRANSFORM_CACHE_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Math/Math.hpp"

#include <cmath>

namespace Renderer
{
namespace Temporal
{

struct CachedMotionInverseTransform
{
    Math::Vec3 position,
               scale;

    float sin_z = 0.0f,
          cos_z = 1.0f,
          sin_x = 0.0f,
          cos_x = 1.0f,
          sin_y = 0.0f,
          cos_y = 1.0f;
};

inline CachedMotionInverseTransform cacheMotionInverseTransform(
            const Ecs::TransformComponent& transform
    )
{
    const float z = Math::radians(-transform.rotation.z),
                x = Math::radians(-transform.rotation.x),
                y = Math::radians(-transform.rotation.y);

    CachedMotionInverseTransform result;
    result.position = {
        transform.position.x,
        transform.position.y,
        transform.position.z,
    };
    result.scale = {
        transform.scale.x,
        transform.scale.y,
        transform.scale.z,
    };

    result.sin_z = std::sin(z);
    result.cos_z = std::cos(z);
    result.sin_x = std::sin(x);
    result.cos_x = std::cos(x);
    result.sin_y = std::sin(y);
    result.cos_y = std::cos(y);
    return result;
}

inline Math::Vec3 inverseTransformPointMotionCached(
            const Math::Vec3& value,
            const CachedMotionInverseTransform& transform
    )
{
    constexpr float EPSILON = 0.000001f;

    Math::Vec3 result = Math::subtract(value, transform.position);

    const float z_x = result.x * transform.cos_z - result.y * transform.sin_z,
                z_y = result.x * transform.sin_z + result.y * transform.cos_z;
    result.x = z_x;
    result.y = z_y;

    const float x_y = result.y * transform.cos_x - result.z * transform.sin_x,
                x_z = result.y * transform.sin_x + result.z * transform.cos_x;
    result.y = x_y;
    result.z = x_z;

    const float y_x = result.x * transform.cos_y + result.z * transform.sin_y,
                y_z = -result.x * transform.sin_y + result.z * transform.cos_y;
    result.x = y_x;
    result.z = y_z;

    if (std::fabs(transform.scale.x) > EPSILON) result.x /= transform.scale.x;
    if (std::fabs(transform.scale.y) > EPSILON) result.y /= transform.scale.y;
    if (std::fabs(transform.scale.z) > EPSILON) result.z /= transform.scale.z;
    return result;
}

} // namespace Temporal
} // namespace Renderer

#endif
