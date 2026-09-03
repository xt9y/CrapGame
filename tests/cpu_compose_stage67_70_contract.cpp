#include "Renderer/Lighting/DirectLightPolicy.hpp"
#include "Renderer/ParallelRows.hpp"

#include <atomic>
#include <cassert>
#include <vector>

int main()
{
    using namespace Renderer;

    assert(Lighting::directLightSampleIsPositionIndependent(
            Ecs::LightType::Directional
        ));
    assert(!Lighting::directLightSampleIsPositionIndependent(
            Ecs::LightType::Point
        ));
    assert(!Lighting::directLightSampleIsPositionIndependent(
            Ecs::LightType::Spot
        ));

    std::vector<std::atomic<int>> visits(23);
    for (std::atomic<int>& visit : visits)
    {
        visit.store(0);
    }

    parallelRowsDynamic(23, 12u, [&](int row)
    {
        visits[static_cast<std::size_t>(row)].fetch_add(1);
    });

    for (const std::atomic<int>& visit : visits)
    {
        assert(visit.load() == 1);
    }

    return 0;
}
