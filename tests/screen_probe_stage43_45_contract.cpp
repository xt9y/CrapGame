#include "Renderer/Lumen/Sampling.hpp"
#include "Renderer/Lumen/ScreenProbePolicy.hpp"

#include <cassert>
#include <cmath>

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

    return 0;
}
