#include "Renderer/Gpu/ConvergedFrameCache.hpp"
#include "Renderer/Gpu/ProgressiveTracePolicy.hpp"

#include <cstdlib>
#include <iostream>

static void require (bool value, const char *message)
{
    if (!value)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

int main ()
{
    using namespace Renderer::Gpu;

    RevisionState revisions = {};
    ConvergedFrameCache cache;
    const std::uint32_t slice_scale =
        ProgressiveTracePolicy::STATIONARY_SLICE_COUNT;
    const std::uint32_t target =
        ConvergencePolicy::DEFAULT_SAMPLES * slice_scale;

    require(cache.needsSample(revisions), "first sample required");

    for (std::uint32_t index = 0; index + 1u < target; ++index)
        cache.recordSample(revisions, true);

    require(cache.sampleCount() == target - 1u,
            "progressive slices were not counted toward complete-sweep convergence");
    require(!cache.frozen(revisions),
            "static GI froze before all configured progressive sweeps completed");

    cache.recordSample(revisions, true);
    require(cache.sampleCount() == target,
            "default progressive convergence count is wrong");
    require(cache.frozen(revisions),
            "default static convergence did not freeze after complete sweeps");

    RevisionState moved = revisions;
    ++moved.camera;
    require(!cache.frozen(moved),
            "camera revision wakes final frame");

    cache.begin(revisions);
    const std::uint32_t mandatory =
        ConvergencePolicy::MIN_SAMPLES * slice_scale;
    for (std::uint32_t index = 0; index < mandatory; ++index)
        cache.recordSample(revisions, index + 1u < mandatory);

    require(cache.sampleCount() == mandatory,
            "minimum progressive samples are recorded");
    require(cache.frozen(revisions),
            "history can declare stable after mandatory complete-sweep work");

    cache.invalidate();
    require(cache.needsSample(revisions),
            "explicit invalidation wakes cache");

    std::cout << "converged_frame_cache_contract=PASS\n";
    return 0;
}
