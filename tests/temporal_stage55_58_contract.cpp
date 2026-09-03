#include "Renderer/ParallelRows.hpp"
#include "Renderer/Temporal/MotionTransformCache.hpp"
#include "Renderer/Math/Math.hpp"

#include <atomic>
#include <cassert>
#include <vector>

int main()
{
    using namespace Renderer;
    using namespace Renderer::Temporal;

    Ecs::TransformComponent transform = {};
    transform.position = {1.25f, -2.5f, 3.75f};
    transform.rotation = {13.0f, -27.0f, 41.0f};
    transform.scale = {2.0f, 0.75f, 1.5f};

    const Math::Vec3 point = {3.5f, -0.25f, 5.0f};
    const Math::Vec3 expected = Math::inverseTransformPoint(
            point,
            {transform.position.x, transform.position.y, transform.position.z},
            {transform.rotation.x, transform.rotation.y, transform.rotation.z},
            {transform.scale.x, transform.scale.y, transform.scale.z}
        );
    const Math::Vec3 actual = inverseTransformPointMotionCached(
            point,
            cacheMotionInverseTransform(transform)
        );

    assert(actual.x == expected.x);
    assert(actual.y == expected.y);
    assert(actual.z == expected.z);

    Ecs::TransformComponent zero_scale = transform;
    zero_scale.scale = {0.0f, 0.0000005f, 1.0f};

    const Math::Vec3 zero_expected = Math::inverseTransformPoint(
            point,
            {zero_scale.position.x, zero_scale.position.y, zero_scale.position.z},
            {zero_scale.rotation.x, zero_scale.rotation.y, zero_scale.rotation.z},
            {zero_scale.scale.x, zero_scale.scale.y, zero_scale.scale.z}
        );
    const Math::Vec3 zero_actual = inverseTransformPointMotionCached(
            point,
            cacheMotionInverseTransform(zero_scale)
        );

    assert(zero_actual.x == zero_expected.x);
    assert(zero_actual.y == zero_expected.y);
    assert(zero_actual.z == zero_expected.z);

    std::vector<std::atomic<int>> visits(53);
    for (std::atomic<int>& visit : visits)
    {
        visit.store(0);
    }

    parallelRowsDynamic(53, 32u, [&](int row)
    {
        visits[static_cast<std::size_t>(row)].fetch_add(
                1,
                std::memory_order_relaxed
            );
    });

    for (const std::atomic<int>& visit : visits)
    {
        assert(visit.load(std::memory_order_relaxed) == 1);
    }

    return 0;
}
