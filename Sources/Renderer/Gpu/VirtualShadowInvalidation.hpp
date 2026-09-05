#ifndef CRAPGAME_RENDERER_GPU_VIRTUALSHADOWINVALIDATION_HPP
#define CRAPGAME_RENDERER_GPU_VIRTUALSHADOWINVALIDATION_HPP

#include "Renderer/Gpu/VirtualShadowPolicy.hpp"
#include "Renderer/Math/Math.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer
{
namespace Gpu
{

struct ShadowInvalidationRange
{
    int minimum_x = 0,
        minimum_y = 0,
        maximum_x = 0,
        maximum_y = 0;
};

inline ShadowInvalidationRange shadowInvalidationRange (
            int level,
            const Math::Vec3& minimum,
            const Math::Vec3& maximum,
            const Math::Vec3& right,
            const Math::Vec3& up
    )
{
    const Math::Vec3 center = {
        (minimum.x + maximum.x) * 0.5f,
        (minimum.y + maximum.y) * 0.5f,
        (minimum.z + maximum.z) * 0.5f,
    };
    const Math::Vec3 extent = {
        std::max(0.0f, (maximum.x - minimum.x) * 0.5f),
        std::max(0.0f, (maximum.y - minimum.y) * 0.5f),
        std::max(0.0f, (maximum.z - minimum.z) * 0.5f),
    };

    const auto projection = [](
            const Math::Vec3& value,
            const Math::Vec3& axis)
    {
        return value.x * axis.x + value.y * axis.y + value.z * axis.z;
    };

    const auto radius = [](
            const Math::Vec3& value,
            const Math::Vec3& axis)
    {
        return value.x * std::fabs(axis.x)
            + value.y * std::fabs(axis.y)
            + value.z * std::fabs(axis.z);
    };

    const float page_world_size =
        virtualShadowClipmapExtent(level) * 2.0f /
        static_cast<float>(VirtualShadowPolicy::LEVEL0_PAGES);

    const float center_x = projection(center, right),
                center_y = projection(center, up),
                radius_x = radius(extent, right),
                radius_y = radius(extent, up);

    ShadowInvalidationRange result;
    result.minimum_x = static_cast<int>(std::floor(
            (center_x - radius_x) / page_world_size
        ));
    result.maximum_x = static_cast<int>(std::floor(
            (center_x + radius_x) / page_world_size
        ));
    result.minimum_y = static_cast<int>(std::floor(
            (center_y - radius_y) / page_world_size
        ));
    result.maximum_y = static_cast<int>(std::floor(
            (center_y + radius_y) / page_world_size
        ));
    return result;
}

} // namespace Gpu
} // namespace Renderer

#endif
