#include "Renderer/CpuReferencePolicy.hpp"

#include <cassert>

int main ()
{
    using Renderer::CpuReferenceMode;
    using Renderer::cpuDirectNeedsRefresh;
    using Renderer::cpuGeometryNeedsRefresh;
    using Renderer::cpuReferencePlan;
    using Renderer::cpuWindowPresentationRequired;

    const auto albedo = cpuReferencePlan(CpuReferenceMode::Albedo);
    assert(albedo.geometry);
    assert(!albedo.motion);
    assert(!albedo.tracer);
    assert(!albedo.direct);
    assert(!albedo.screen_probes);
    assert(!albedo.reflections);
    assert(!albedo.final_taa);

    const auto direct = cpuReferencePlan(CpuReferenceMode::Direct);
    assert(direct.geometry);
    assert(direct.shadows);
    assert(direct.direct);
    assert(!direct.tracer);
    assert(!direct.screen_probes);
    assert(!direct.reflections);

    const auto indirect = cpuReferencePlan(CpuReferenceMode::Indirect);
    assert(indirect.geometry);
    assert(indirect.motion);
    assert(indirect.tracer);
    assert(indirect.cards);
    assert(indirect.shadows);
    assert(indirect.surface_cache);
    assert(indirect.scene_lighting);
    assert(indirect.radiosity);
    assert(indirect.radiance_cache);
    assert(indirect.screen_probes);
    assert(indirect.gi_taa);
    assert(!indirect.direct);
    assert(!indirect.reflections);

    const auto reflection = cpuReferencePlan(CpuReferenceMode::Reflection);
    assert(reflection.geometry);
    assert(reflection.motion);
    assert(reflection.tracer);
    assert(reflection.radiance_cache);
    assert(reflection.reflections);
    assert(!reflection.direct);
    assert(!reflection.screen_probes);

    const auto final_plan = cpuReferencePlan(CpuReferenceMode::Final);
    assert(final_plan.geometry);
    assert(final_plan.motion);
    assert(final_plan.tracer);
    assert(final_plan.cards);
    assert(final_plan.shadows);
    assert(final_plan.surface_cache);
    assert(final_plan.scene_lighting);
    assert(final_plan.radiosity);
    assert(final_plan.radiance_cache);
    assert(final_plan.direct);
    assert(final_plan.screen_probes);
    assert(final_plan.gi_taa);
    assert(final_plan.reflections);
    assert(final_plan.final_taa);

    assert(cpuGeometryNeedsRefresh(false, false, false, false));
    assert(cpuGeometryNeedsRefresh(true, true, false, false));
    assert(cpuGeometryNeedsRefresh(true, false, true, false));
    assert(cpuGeometryNeedsRefresh(true, false, false, true));
    assert(!cpuGeometryNeedsRefresh(true, false, false, false));

    assert(cpuDirectNeedsRefresh(false, false, false));
    assert(cpuDirectNeedsRefresh(true, true, false));
    assert(cpuDirectNeedsRefresh(true, false, true));
    assert(!cpuDirectNeedsRefresh(true, false, false));

    assert(cpuWindowPresentationRequired(false));
    assert(!cpuWindowPresentationRequired(true));

    return 0;
}
