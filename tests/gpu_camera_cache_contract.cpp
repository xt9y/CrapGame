#include "Renderer/Gpu/CameraCache.hpp"

#include <cassert>

int main()
{
    using Renderer::Gpu::shouldUpdateCameraMatrices;

    assert(shouldUpdateCameraMatrices(false, false, false));
    assert(shouldUpdateCameraMatrices(true, true, false));
    assert(shouldUpdateCameraMatrices(true, false, true));
    assert(!shouldUpdateCameraMatrices(true, false, false));
    return 0;
}
