#ifndef CRAPGAME_RENDERER_LUMEN_SCREEN_PROBE_POLICY_HPP
#define CRAPGAME_RENDERER_LUMEN_SCREEN_PROBE_POLICY_HPP

#include <algorithm>

namespace Renderer
{
namespace Lumen
{

inline unsigned screenProbeWorkerCount(int work_items, unsigned hardware_threads)
{
    if (work_items <= 1)
    {
        return 1u;
    }

    constexpr unsigned MAX_PROBE_WORKERS = 16u;
    const unsigned available = std::min(
            MAX_PROBE_WORKERS,
            std::max(1u, hardware_threads)
        );

    return std::min(
            static_cast<unsigned>(work_items),
            available
        );
}

} // namespace Lumen
} // namespace Renderer

#endif
