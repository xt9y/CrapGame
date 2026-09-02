#include "Budget.hpp"

#include <algorithm>
#include <cstddef>

namespace Renderer 
{
namespace Lumen 
{

FrameBudget budgetForFrame (
                int width,
                int height,
                std::uint64_t frame_index
        ) 
{
    const int render_width = std::max(1, width),
              render_height = std::max(1, height);

    const std::size_t pixel_count =
        static_cast<std::size_t>(render_width) *
        static_cast<std::size_t>(render_height);

    FrameBudget budget = {
        12u,
        16,
        8,
        0.35f,
    };

    if (pixel_count > 1280u * 900u) 
    {
        budget.radiance_probes_per_frame = 8u;
        budget.screen_probe_spacing = 20;
        budget.screen_probe_rays = 6;
        budget.radiosity_feedback = 0.30f;
    }

    if (pixel_count > 1920u * 1080u) 
    {
        budget.radiance_probes_per_frame = 6u;
        budget.screen_probe_spacing = 24;
        budget.screen_probe_rays = 4;
        budget.radiosity_feedback = 0.25f;
    }

    if (frame_index > 0u
            && frame_index % 4u == 0u) 
    {
        budget.radiance_probes_per_frame += 2u;
    }

    return budget;
}

} // namespace Lumen
} // namespace Renderer
