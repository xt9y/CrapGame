#ifndef CRAPGAME_RENDERER_GPU_FRAME_HOT_PATH_HPP
#define CRAPGAME_RENDERER_GPU_FRAME_HOT_PATH_HPP

namespace Renderer
{
namespace Gpu
{

inline bool gpuWorkInvalidatesPresenter (
            bool geometry_ran,
            bool direct_ran,
            bool lumen_ran
    )
{
    return geometry_ran || direct_ran || lumen_ran;
}

inline bool conservativeGpuCleanupRequired (
            bool next_consumer_explicitly_rebinds
    )
{
    return !next_consumer_explicitly_rebinds;
}

inline bool frameEndClockRequired (bool performance_mode)
{
    return performance_mode;
}

inline bool frameCaptureRequired (bool renderercheck_mode)
{
    return renderercheck_mode;
}

inline bool visualRunTimingRequired (
            bool renderercheck_mode,
            bool performance_mode,
            bool metrics_path_available
    )
{
    return renderercheck_mode
        && !performance_mode
        && metrics_path_available;
}

} // namespace Gpu
} // namespace Renderer

#endif
