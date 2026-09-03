#include "Renderer/Gpu/FrameHotPath.hpp"

#include <cassert>

int main ()
{
    using namespace Renderer::Gpu;

    assert(!gpuWorkInvalidatesPresenter(false, false, false));
    assert(gpuWorkInvalidatesPresenter(true, false, false));
    assert(gpuWorkInvalidatesPresenter(false, true, false));
    assert(gpuWorkInvalidatesPresenter(false, false, true));

    assert(!frameEndClockRequired(false));
    assert(frameEndClockRequired(true));

    assert(!frameCaptureRequired(false));
    assert(frameCaptureRequired(true));

    return 0;
}
