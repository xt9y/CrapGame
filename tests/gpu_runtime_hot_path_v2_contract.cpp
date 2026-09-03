#include "Renderer/Gpu/RuntimeHotPath.hpp"

#include <cassert>

int main()
{
    using namespace Renderer::Gpu;

    assert(windowMaintenanceDue(false, 1000000ull, 0ull));
    assert(!windowMaintenanceDue(false, 1500000ull, 1000000ull));
    assert(windowMaintenanceDue(false, 2000000ull, 1000000ull));
    assert(!windowMaintenanceDue(true, 2000000ull, 0ull));

    assert(resizeCheckRequired(false, false));
    assert(!resizeCheckRequired(true, false));
    assert(!resizeCheckRequired(false, true));

    assert(!profilerCallChainRequired(false));
    assert(profilerCallChainRequired(true));

    return 0;
}
