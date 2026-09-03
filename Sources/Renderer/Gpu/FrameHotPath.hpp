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

inline bool frameEndClockRequired (bool performance_mode)
{
    return performance_mode;
}

inline bool frameCaptureRequired (bool renderercheck_mode)
{
    return renderercheck_mode;
}

} // namespace Gpu
} // namespace Renderer

#endif
