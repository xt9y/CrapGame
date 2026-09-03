#include "Renderer/Gpu/ResourceLifecycle.hpp"

#include <cassert>

int main ()
{
    using namespace Renderer::Gpu;

    assert(normalizedExtent(1280) == 1280);
    assert(normalizedExtent(0) == 1);
    assert(normalizedExtent(-10) == 1);

    assert(!resizeStorageRequired(1280, 870, true, 1280, 870));
    assert(resizeStorageRequired(1280, 870, false, 1280, 870));
    assert(resizeStorageRequired(1280, 870, true, 1920, 1080));
    assert(!resizeStorageRequired(1, 1, true, 0, -4));
    assert(resizeStorageRequired(1280, 870, true, 0, -4));
    return 0;
}
