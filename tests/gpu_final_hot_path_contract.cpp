#include "Renderer/Gpu/RuntimeHotPathV3.hpp"

#include <cassert>
#include <cstdint>

int main ()
{
    using namespace Renderer::Gpu;

    std::uint64_t phase = 0u;
    assert(simulationTicksDue(16666666ull, &phase) == 0u);
    assert(simulationTicksDue(1ull, &phase) == 1u);
    assert(phase == 20ull);

    phase = 0u;
    assert(simulationTicksDue(1000000000ull, &phase) == 15u);
    assert(phase == 0u);

    assert(!statsReportDue(1999999999ull, 0ull));
    assert(statsReportDue(2000000000ull, 0ull));
    assert(statsReportDue(100ull, 200ull));

    assert(rendererRevisionNeedsUpdate(false, 7ull, 7ull));
    assert(!rendererRevisionNeedsUpdate(true, 7ull, 7ull));
    assert(rendererRevisionNeedsUpdate(true, 8ull, 7ull));

    return 0;
}
