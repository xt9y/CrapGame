#include "Renderer/Lumen/ParallelRows.hpp"
#include "Renderer/Lumen/ReflectionPolicy.hpp"
#include "Renderer/Lumen/ScreenTracePolicy.hpp"
#include "Renderer/Lumen/SdfTransformCache.hpp"
#include "Renderer/Lumen/Tracer.hpp"
#include "Renderer/Math/Math.hpp"
#include "Renderer/ParallelRows.hpp"

#include <atomic>
#include <cassert>
#include <cmath>
#include <limits>
#include <type_traits>
#include <vector>

int main()
{
    using namespace Renderer;
    using namespace Renderer::Lumen;

    const auto visibility_trace = &Tracer::traceVisibility;
    (void)visibility_trace;

    assert(screenTraceWithinThicknessSquared(
            {0.10f, 0.10f, 0.00f},
            0.15f
        ));
    assert(!screenTraceWithinThicknessSquared(
            {0.20f, 0.00f, 0.00f},
            0.15f
        ));

    Ecs::TransformComponent transform = {};
    transform.position = {1.0f, -2.0f, 3.0f};
    transform.rotation = {13.0f, -27.0f, 41.0f};
    transform.scale = {2.0f, 0.75f, 1.5f};

    const Math::Vec3 point = {3.5f, -0.25f, 5.0f};
    const CachedInverseTransform cached = cacheInverseTransform(transform);
    const Math::Vec3 expected = Math::inverseTransformPoint(
            point,
            {transform.position.x, transform.position.y, transform.position.z},
            {transform.rotation.x, transform.rotation.y, transform.rotation.z},
            {transform.scale.x, transform.scale.y, transform.scale.z}
        );
    const Math::Vec3 actual = inverseTransformPointCached(point, cached);

    assert(std::fabs(actual.x - expected.x) <= 0.000001f);
    assert(std::fabs(actual.y - expected.y) <= 0.000001f);
    assert(std::fabs(actual.z - expected.z) <= 0.000001f);

    assert(Renderer::parallelWorkerCount(0, 8u) == 1u);
    assert(Renderer::parallelWorkerCount(64, 0u) == 1u);
    assert(Renderer::parallelWorkerCount(64, 32u) == 16u);
    assert(Renderer::parallelWorkerCount(7, 32u) == 7u);

    std::vector<std::atomic<int>> visits(37);
    for (std::atomic<int>& visit : visits)
    {
        visit.store(0);
    }

    Renderer::parallelRowsDynamic(37, 8u, [&](int row)
    {
        visits[static_cast<std::size_t>(row)].fetch_add(1);
    });

    for (const std::atomic<int>& visit : visits)
    {
        assert(visit.load() == 1);
    }

    std::atomic<int> lumen_visits{0};
    Renderer::Lumen::parallelRowsDynamic(9, 4u, [&](int)
    {
        lumen_visits.fetch_add(1);
    });
    assert(lumen_visits.load() == 9);

    assert(reflectionUsesDetailedTrace(0.0f));
    assert(reflectionUsesDetailedTrace(0.349999f));
    assert(!reflectionUsesDetailedTrace(0.35f));
    assert(!reflectionUsesDetailedTrace(1.0f));
    assert(reflectionUsesDetailedTrace(
            std::numeric_limits<float>::quiet_NaN()
        ));

    int hit_calls = 0;
    int miss_calls = 0;

    const int hit_value = sampleReflectionRadiance(
            true,
            [&]()
            {
                ++hit_calls;
                return 17;
            },
            [&]()
            {
                ++miss_calls;
                return 29;
            }
        );

    assert(hit_value == 17);
    assert(hit_calls == 1);
    assert(miss_calls == 0);

    const int miss_value = sampleReflectionRadiance(
            false,
            [&]()
            {
                ++hit_calls;
                return 17;
            },
            [&]()
            {
                ++miss_calls;
                return 29;
            }
        );

    assert(miss_value == 29);
    assert(hit_calls == 1);
    assert(miss_calls == 1);

    return 0;
}
