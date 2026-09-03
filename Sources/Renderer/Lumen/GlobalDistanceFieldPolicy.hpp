#ifndef CRAPGAME_RENDERER_LUMEN_GLOBAL_DISTANCE_FIELD_POLICY_HPP
#define CRAPGAME_RENDERER_LUMEN_GLOBAL_DISTANCE_FIELD_POLICY_HPP

#include "Renderer/Math/Math.hpp"

namespace Renderer
{
namespace Lumen
{

inline float gdfClampExact(float value, float minimum, float maximum)
{
    if (value < minimum)
    {
        return minimum;
    }

    if (value > maximum)
    {
        return maximum;
    }

    return value;
}

inline bool gdfContainsExact(
            const Math::Vec3& center,
            float half_extent,
            const Math::Vec3& position
    )
{
    const float dx = position.x - center.x,
                dy = position.y - center.y,
                dz = position.z - center.z;

    return dx >= -half_extent && dx <= half_extent
        && dy >= -half_extent && dy <= half_extent
        && dz >= -half_extent && dz <= half_extent;
}

} // namespace Lumen
} // namespace Renderer

#endif
