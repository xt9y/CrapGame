#include "Renderer/Lumen/AoSamplingMath.hpp"
#include "Renderer/Lumen/Sampling.hpp"

#include <cassert>

namespace
{

void assertVec3Exact(
            const Renderer::Math::Vec3& actual,
            const Renderer::Math::Vec3& expected
    )
{
    assert(actual.x == expected.x);
    assert(actual.y == expected.y);
    assert(actual.z == expected.z);
}

void assertBasisExact(
            const Renderer::Lumen::HemisphereBasis& actual,
            const Renderer::Lumen::HemisphereBasis& expected
    )
{
    assertVec3Exact(actual.normal, expected.normal);
    assertVec3Exact(actual.tangent, expected.tangent);
    assertVec3Exact(actual.bitangent, expected.bitangent);
}

} // namespace

int main()
{
    using namespace Renderer;
    using namespace Renderer::Lumen;

    const Math::Vec3 normals[] = {
        {0.0f, 1.0f, 0.0f},
        {0.31f, 0.77f, -0.22f},
        {-0.42f, 0.12f, 0.88f},
    };

    const HemisphereSample samples[] = {
        {0.17f, 0.93f, -0.21f},
        {-0.44f, 0.61f, 0.52f},
        {0.0f, 1.0f, 0.0f},
    };

    for (const Math::Vec3& normal : normals)
    {
        const HemisphereBasis expected_basis = hemisphereBasis(normal);
        const HemisphereBasis actual_basis = aoHemisphereBasisExact(normal);
        assertBasisExact(actual_basis, expected_basis);

        for (const HemisphereSample& sample : samples)
        {
            assertVec3Exact(
                    aoSampleHemisphereExact(actual_basis, sample),
                    sampleHemisphere(expected_basis, sample)
                );
        }
    }

    assert(aoRadianceIsExactlyZero({0.0f, -0.0f, 0.0f}));
    assert(!aoRadianceIsExactlyZero({0.0f, 0.000001f, 0.0f}));

    return 0;
}
