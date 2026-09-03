#ifndef CRAPGAME_RENDERER_LUMEN_SDF_TRANSFORM_CACHE_HPP
#define CRAPGAME_RENDERER_LUMEN_SDF_TRANSFORM_CACHE_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Math/Math.hpp"

#include <cmath>

namespace Renderer
{
namespace Lumen
{

struct CachedInverseTransform
{
    Math::Vec3 position;
    Math::Vec3 inverse_scale;

    float sin_z = 0.0f,
          cos_z = 1.0f,
          sin_x = 0.0f,
          cos_x = 1.0f,
          sin_y = 0.0f,
          cos_y = 1.0f;
};

inline CachedInverseTransform cacheInverseTransform(
            const Ecs::TransformComponent& transform
    )
{
    constexpr float EPSILON = 0.000001f;

    const float z = Math::radians(-transform.rotation.z),
                x = Math::radians(-transform.rotation.x),
                y = Math::radians(-transform.rotation.y);

    CachedInverseTransform result;
    result.position = {
        transform.position.x,
        transform.position.y,
        transform.position.z,
    };

    result.inverse_scale = {
        std::fabs(transform.scale.x) > EPSILON ? 1.0f / transform.scale.x : 1.0f,
        std::fabs(transform.scale.y) > EPSILON ? 1.0f / transform.scale.y : 1.0f,
        std::fabs(transform.scale.z) > EPSILON ? 1.0f / transform.scale.z : 1.0f,
    };

    result.sin_z = std::sin(z);
    result.cos_z = std::cos(z);
    result.sin_x = std::sin(x);
    result.cos_x = std::cos(x);
    result.sin_y = std::sin(y);
    result.cos_y = std::cos(y);
    return result;
}

inline Math::Vec3 inverseTransformPointCached(
            const Math::Vec3& value,
            const CachedInverseTransform& transform
    )
{
    Math::Vec3 result = Math::subtract(value, transform.position);

    const float z_x = transform.cos_z * result.x - transform.sin_z * result.y,
                z_y = transform.sin_z * result.x + transform.cos_z * result.y;
    result.x = z_x;
    result.y = z_y;

    const float x_y = transform.cos_x * result.y - transform.sin_x * result.z,
                x_z = transform.sin_x * result.y + transform.cos_x * result.z;
    result.y = x_y;
    result.z = x_z;

    const float y_x = transform.cos_y * result.x + transform.sin_y * result.z,
                y_z = -transform.sin_y * result.x + transform.cos_y * result.z;
    result.x = y_x;
    result.z = y_z;

    result.x *= transform.inverse_scale.x;
    result.y *= transform.inverse_scale.y;
    result.z *= transform.inverse_scale.z;
    return result;
}

} // namespace Lumen
} // namespace Renderer

#endif
