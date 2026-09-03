#include "Renderer/Gpu/FrameHotPath.hpp"

#include <cassert>

int main ()
{
    using namespace Renderer::Gpu;

    assert(!gpuWorkInvalidatesPresenter(false, false, false));
    assert(gpuWorkInvalidatesPresenter(true, false, false));
    assert(gpuWorkInvalidatesPresenter(false, true, false));
    assert(gpuWorkInvalidatesPresenter(false, false, true));

    assert(!conservativeGpuCleanupRequired(true));
    assert(conservativeGpuCleanupRequired(false));

    assert(!frameEndClockRequired(false));
    assert(frameEndClockRequired(true));

    assert(!frameCaptureRequired(false));
    assert(frameCaptureRequired(true));

    assert(!visualRunTimingRequired(false, false, true));
    assert(!visualRunTimingRequired(true, true, true));
    assert(!visualRunTimingRequired(true, false, false));
    assert(visualRunTimingRequired(true, false, true));

    return 0;
}
