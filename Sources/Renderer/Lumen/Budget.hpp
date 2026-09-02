#ifndef CRAPGAME_RENDERER_LUMEN_BUDGET_HPP
#define CRAPGAME_RENDERER_LUMEN_BUDGET_HPP

#include <cstdint>

namespace Renderer 
{
namespace Lumen 
{

struct FrameBudget 
{
    unsigned int radiance_probes_per_frame;

    int screen_probe_spacing,
        screen_probe_rays;

    float radiosity_feedback;
};

FrameBudget budgetForFrame (
                int width,
                int height,
                std::uint64_t frame_index
        );

} // namespace Lumen
} // namespace Renderer

#endif
