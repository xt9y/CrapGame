#ifndef CRAPGAME_RENDERER_GPU_RUNTIME_HOT_PATH_V3_HPP
#define CRAPGAME_RENDERER_GPU_RUNTIME_HOT_PATH_V3_HPP

#include <cstdint>

namespace Renderer
{
namespace Gpu
{

constexpr std::uint64_t SIMULATION_RATE_HZ = 60ull;
constexpr std::uint64_t SIMULATION_PHASE_PER_TICK = 1000000000ull;
constexpr std::uint64_t MAX_SIMULATION_DELTA_NS = 250000000ull;
constexpr std::uint64_t STATS_REPORT_INTERVAL_NS = 2000000000ull;

inline std::uint32_t simulationTicksDue (
                std::uint64_t delta_ns,
                std::uint64_t *phase
        )
{
    if (!phase)
    {
        return 0u;
    }

    if (delta_ns > MAX_SIMULATION_DELTA_NS)
    {
        delta_ns = MAX_SIMULATION_DELTA_NS;
    }

    const std::uint64_t accumulated =
        *phase + delta_ns * SIMULATION_RATE_HZ;
    const std::uint32_t ticks = static_cast<std::uint32_t>(
            accumulated / SIMULATION_PHASE_PER_TICK
        );

    *phase = accumulated % SIMULATION_PHASE_PER_TICK;
    return ticks;
}

inline bool statsReportDue (
                std::uint64_t now_ns,
                std::uint64_t window_start_ns
        )
{
    return now_ns < window_start_ns
        || now_ns - window_start_ns >= STATS_REPORT_INTERVAL_NS;
}

inline bool rendererRevisionNeedsUpdate (
                bool revision_valid,
                std::uint64_t world_revision,
                std::uint64_t cached_revision
        )
{
    return !revision_valid || world_revision != cached_revision;
}

} // namespace Gpu
} // namespace Renderer

#endif
