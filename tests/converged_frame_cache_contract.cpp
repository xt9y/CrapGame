#include "Renderer/Gpu/ConvergedFrameCache.hpp"

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

    require(cache.needsSample(revisions), "first sample required");

    for (int index = 0; index < 8; ++index)
        cache.recordSample(revisions, true);

    require(cache.sampleCount() == 8u,
            "default path records eight samples");
    require(cache.frozen(revisions),
            "default static convergence freezes at eight");

    RevisionState moved = revisions;
    ++moved.camera;
    require(!cache.frozen(moved),
            "camera revision wakes final frame");

    cache.begin(revisions);
    for (int index = 0; index < 4; ++index)
        cache.recordSample(revisions, index < 3);

    require(cache.sampleCount() == 4u,
            "minimum samples are recorded");
    require(cache.frozen(revisions),
            "history can declare stable after mandatory samples");

    cache.invalidate();
    require(cache.needsSample(revisions),
            "explicit invalidation wakes cache");

    std::cout << "converged_frame_cache_contract=PASS\n";
    return 0;
}
