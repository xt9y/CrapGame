#include "Renderer/CpuReferencePolicy.hpp"

#include <cassert>

int main ()
{
    using Renderer::CpuReferenceMode;
    using Renderer::cpuMotionNeedsRefresh;
    using Renderer::cpuReferencePlan;

    const auto surface_lighting =
        cpuReferencePlan(CpuReferenceMode::SurfaceLighting);

    assert(surface_lighting.geometry);
    assert(surface_lighting.tracer);
    assert(surface_lighting.cards);
    assert(surface_lighting.shadows);
    assert(surface_lighting.surface_cache);
    assert(surface_lighting.scene_lighting);
    assert(surface_lighting.radiosity);
    assert(surface_lighting.radiance_cache);
    assert(!surface_lighting.direct);
    assert(!surface_lighting.screen_probes);
    assert(!surface_lighting.reflections);

    assert(cpuMotionNeedsRefresh(false, false, false, false));
    assert(cpuMotionNeedsRefresh(true, true, false, false));
    assert(cpuMotionNeedsRefresh(true, false, true, false));
    assert(cpuMotionNeedsRefresh(true, false, false, true));
    assert(!cpuMotionNeedsRefresh(true, false, false, false));

    return 0;
}
