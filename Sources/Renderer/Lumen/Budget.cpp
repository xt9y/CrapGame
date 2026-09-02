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

    /*
     * Small deterministic captures retain the original Stage-35 quality.
     * Interactive resolutions must not launch software ray workloads at the
     * same density: those traces are the temporary path until their GL43
     * compute replacements land.
     */
    FrameBudget budget = {
        12u,
        16,
        8,
        0.35f,
    };

    if (pixel_count > 640u * 360u)
    {
        budget.radiance_probes_per_frame = 8u;
        budget.screen_probe_spacing = 24;
        budget.screen_probe_rays = 4;
        budget.radiosity_feedback = 0.30f;
    }

    if (pixel_count > 960u * 540u)
    {
        budget.radiance_probes_per_frame = 4u;
        budget.screen_probe_spacing = 32;
        budget.screen_probe_rays = 2;
        budget.radiosity_feedback = 0.25f;
    }

    if (pixel_count > 1920u * 1080u)
    {
        budget.radiance_probes_per_frame = 3u;
        budget.screen_probe_spacing = 40;
        budget.screen_probe_rays = 1;
        budget.radiosity_feedback = 0.20f;
    }

    if (frame_index > 0u
            && frame_index % 8u == 0u)
    {
        budget.radiance_probes_per_frame += 1u;
    }

    return budget;
}

} // namespace Lumen
} // namespace Renderer
