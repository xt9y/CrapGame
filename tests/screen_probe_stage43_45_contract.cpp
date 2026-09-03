#include "Renderer/Lumen/Sampling.hpp"
#include "Renderer/Lumen/ScreenProbePolicy.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <vector>

namespace
{
bool near(float a, float b)
{
    return std::fabs(a - b) <= 0.000001f;
}
}

int main()
{
    using namespace Renderer;

    assert(Lumen::screenProbeWorkerCount(1, 8) == 1u);
    assert(Lumen::screenProbeWorkerCount(8, 1) == 1u);
    assert(Lumen::screenProbeWorkerCount(8, 4) == 4u);
    assert(Lumen::screenProbeWorkerCount(3, 16) == 3u);
    assert(Lumen::screenProbeWorkerCount(100, 64) == 16u);

    const Math::Vec3 normal = {0.0f, 1.0f, 0.0f};
    const auto basis = Lumen::hemisphereBasis(normal);
    const auto sequence = Lumen::buildHemisphereSequence(4, 12u);
    assert(sequence.size() == 4u);

    for (int i = 0; i < 4; ++i)
    {
        const Math::Vec3 cached = Lumen::sampleHemisphere(
                basis,
                sequence[static_cast<std::size_t>(i)]
            );
        const Math::Vec3 legacy = Lumen::sampleHemisphere(normal, i, 4, 12u);
        assert(near(cached.x, legacy.x));
        assert(near(cached.y, legacy.y));
        assert(near(cached.z, legacy.z));
    }

    std::vector<Lumen::HemisphereSample> reusable(32u);
    Lumen::fillHemisphereSequence(4, 12u, &reusable);
    assert(reusable.size() == sequence.size());

    for (std::size_t index = 0; index < sequence.size(); ++index)
    {
        assert(reusable[index].x == sequence[index].x);
        assert(reusable[index].y == sequence[index].y);
        assert(reusable[index].z == sequence[index].z);
    }

    const std::size_t stable_capacity = reusable.capacity();
    Lumen::fillHemisphereSequence(4, 13u, &reusable);
    assert(reusable.size() == 4u);
    assert(reusable.capacity() == stable_capacity);

    const auto next_sequence = Lumen::buildHemisphereSequence(4, 13u);
    for (std::size_t index = 0; index < next_sequence.size(); ++index)
    {
        assert(reusable[index].x == next_sequence[index].x);
        assert(reusable[index].y == next_sequence[index].y);
        assert(reusable[index].z == next_sequence[index].z);
    }

    Lumen::fillHemisphereSequence(0, 99u, &reusable);
    assert(reusable.size() == 1u);
    const auto clamped_sequence = Lumen::buildHemisphereSequence(0, 99u);
    assert(reusable[0].x == clamped_sequence[0].x);
    assert(reusable[0].y == clamped_sequence[0].y);
    assert(reusable[0].z == clamped_sequence[0].z);

    Lumen::fillHemisphereSequence(4, 12u, nullptr);

    return 0;
}
