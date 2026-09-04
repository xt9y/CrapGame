#ifndef CRAPGAME_RENDERER_GPU_REPROJECTIONPOLICY_HPP
#define CRAPGAME_RENDERER_GPU_REPROJECTIONPOLICY_HPP

#include <algorithm>
#include <cstdint>

namespace Renderer
{
namespace Gpu
{

struct ReprojectionValidationInput
{
    float projected_u = 0.0f;
    float projected_v = 0.0f;
    bool previous_depth_valid = false;
    std::uint32_t current_material_id = 0u;
    std::uint32_t previous_material_id = 0u;
    float position_error = 0.0f;
    float camera_distance = 0.0f;
    float normal_dot = -1.0f;
};

struct ReprojectionPolicy
{
    static constexpr float NORMAL_DOT_MIN = 0.94f;

    static float positionTolerance(float camera_distance)
    {
        return std::max(0.03f, 0.01f * std::max(camera_distance, 0.0f));
    }

    static bool accepts(const ReprojectionValidationInput& sample)
    {
        if (sample.projected_u < 0.0f || sample.projected_u > 1.0f
                || sample.projected_v < 0.0f || sample.projected_v > 1.0f)
        {
            return false;
        }
        if (!sample.previous_depth_valid)
        {
            return false;
        }
        if (sample.current_material_id != sample.previous_material_id)
        {
            return false;
        }
        if (sample.position_error > positionTolerance(sample.camera_distance))
        {
            return false;
        }
        return sample.normal_dot >= NORMAL_DOT_MIN;
    }
};

} // namespace Gpu
} // namespace Renderer

#endif
