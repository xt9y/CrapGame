#include "Renderer/Gpu/BvhBench.hpp"
#include "Renderer/Gpu/LumenSchedule.hpp"

#include <cassert>
#include <cstdint>
#include <cstdlib>

int main ()
{
#if !defined(_WIN32)
    unsetenv("CRAPGAME_BVH_THRESHOLD");
    unsetenv("CRAPGAME_LUMEN_HZ");
#endif

    using Renderer::Gpu::BvhMode;
    using Renderer::Gpu::LumenSchedule;

    assert(Renderer::Gpu::configuredBvhAutoThreshold(8u) == 16u);
    assert(!Renderer::Gpu::shouldUseBvh(BvhMode::Auto, 16u, 8u));
    assert(Renderer::Gpu::shouldUseBvh(BvhMode::Auto, 17u, 8u));
    assert(!Renderer::Gpu::shouldUseBvh(BvhMode::Linear, 256u, 8u));
    assert(Renderer::Gpu::shouldUseBvh(BvhMode::Bvh, 1u, 8u));

    constexpr std::uint64_t T0 = 1000000000ull;
    LumenSchedule adaptive(0u);

    assert(adaptive.due(T0, false));
    assert(adaptive.currentHz(T0) == 240u);
    adaptive.markUpdated(T0);

    assert(!adaptive.due(T0 + 1000000ull, false));
    assert(adaptive.due(T0 + 4166666ull, false));

    assert(adaptive.currentHz(T0 + 249999999ull) == 240u);
    assert(adaptive.currentHz(T0 + 250000000ull) == 120u);
    assert(adaptive.currentHz(T0 + 1000000000ull) == 60u);
    assert(adaptive.currentHz(T0 + 3000000000ull) == 30u);

    const std::uint64_t changed = T0 + 4000000000ull;
    assert(adaptive.due(changed, true));
    assert(adaptive.currentHz(changed) == 240u);
    adaptive.markUpdated(changed);
    assert(!adaptive.due(changed + 1000000ull, false));

    LumenSchedule fixed(90u);
    assert(fixed.due(T0, false));
    fixed.markUpdated(T0);
    assert(fixed.currentHz(T0 + 10000000000ull) == 90u);

#if !defined(_WIN32)
    setenv("CRAPGAME_LUMEN_HZ", "75", 1);
    LumenSchedule configured;
    assert(configured.fixedHz() == 75u);
    unsetenv("CRAPGAME_LUMEN_HZ");
#endif

    return 0;
}
