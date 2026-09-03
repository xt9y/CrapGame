#include "Renderer/Gpu/RuntimeHotPath.hpp"

#include <cstdint>

int main()
{
    using namespace Renderer::Gpu;

    if (!cameraDataNeedsRefresh(false, false)) return 1;
    if (!cameraDataNeedsRefresh(true, true)) return 2;
    if (cameraDataNeedsRefresh(true, false)) return 3;

    if (!resizePollDue(false, 1000000ull, 0ull)) return 4;
    if (resizePollDue(true, 1000000ull, 0ull)) return 5;
    if (resizePollDue(false, 1500000ull, 1000000ull)) return 6;
    if (!resizePollDue(false, 2000000ull, 1000000ull)) return 7;

    if (!gpuProfilerRequested(true, false)) return 8;
    if (!gpuProfilerRequested(false, true)) return 9;
    if (gpuProfilerRequested(false, false)) return 10;

    if (!presentTimerQueryDue(true, 0ull)) return 11;
    if (presentTimerQueryDue(true, 1ull)) return 12;
    if (!presentTimerQueryDue(true, 16ull)) return 13;
    if (!presentTimerQueryDue(false, 1ull)) return 14;

    if (profilerSlotShouldPend(false)) return 15;
    if (!profilerSlotShouldPend(true)) return 16;

    return 0;
}
