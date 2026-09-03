#ifndef CRAPGAME_RENDERER_GPU_RUNTIME_HOT_PATH_HPP
#define CRAPGAME_RENDERER_GPU_RUNTIME_HOT_PATH_HPP

#include <cstdint>

namespace Renderer
{
namespace Gpu
{

constexpr std::uint64_t WINDOW_MAINTENANCE_INTERVAL_NS = 1000000ull;
constexpr std::uint64_t RESIZE_POLL_INTERVAL_NS = WINDOW_MAINTENANCE_INTERVAL_NS;
constexpr std::uint64_t PERF_PRESENT_QUERY_INTERVAL = 16ull;

inline bool cameraDataNeedsRefresh (
                bool cache_valid,
                bool camera_changed
        )
{
    return !cache_valid || camera_changed;
}

inline bool resizePollDue (
                bool fixed_size_window,
                std::uint64_t now_ns,
                std::uint64_t last_poll_ns
        )
{
    if (fixed_size_window)
    {
        return false;
    }

    return last_poll_ns == 0ull
        || now_ns < last_poll_ns
        || now_ns - last_poll_ns >= RESIZE_POLL_INTERVAL_NS;
}

/* RendererCheck's legacy Display.update() still owns event polling. Normal
 * interactive/perf rendering uses swap-only display updates and pumps events
 * separately at up to 1 kHz. */
inline bool windowMaintenanceDue (
                bool renderercheck_mode,
                std::uint64_t now_ns,
                std::uint64_t last_poll_ns
        )
{
    if (renderercheck_mode)
    {
        return false;
    }

    return last_poll_ns == 0ull
        || now_ns < last_poll_ns
        || now_ns - last_poll_ns >= WINDOW_MAINTENANCE_INTERVAL_NS;
}

inline bool resizeCheckRequired (
                bool renderercheck_mode,
                bool performance_mode
        )
{
    return !renderercheck_mode && !performance_mode;
}

inline bool gpuProfilerRequested (
                bool performance_mode,
                bool interactive_requested
        )
{
    return performance_mode || interactive_requested;
}

inline bool profilerCallChainRequired (bool profiler_enabled)
{
    return profiler_enabled;
}

inline bool presentTimerQueryDue (
                bool performance_mode,
                std::uint64_t frame_index
        )
{
    return !performance_mode
        || frame_index % PERF_PRESENT_QUERY_INTERVAL == 0ull;
}

inline bool profilerSlotShouldPend (bool measured_any)
{
    return measured_any;
}

} // namespace Gpu
} // namespace Renderer

#endif
