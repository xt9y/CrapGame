#include "Renderer/GBuffer/GBuffer.hpp"
#include "Renderer/Lumen/AoSamplingMath.hpp"
#include "Renderer/Lumen/Sampling.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <type_traits>
#include <utility>

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

    static_assert(std::is_same_v<
        decltype(std::declval<const GBuffer::Buffer&>().pixelCount()),
        std::size_t
    >);

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

    const std::array<Math::Vec3, 4> positions = {{
        {0.0f, 0.0f, 0.0f},
        {1.25f, -3.5f, 7.75f},
        {-12.0f, 0.125f, 4.0f},
        {0.001f, -0.002f, 0.003f},
    }};

    const std::array<float, 4> offsets = {{
        0.0f,
        0.025f,
        0.04f,
        1.25f,
    }};

    for (const Math::Vec3& position : positions)
    {
        for (const Math::Vec3& normal : normals)
        {
            for (const float offset : offsets)
            {
                assertVec3Exact(
                    aoOffsetOriginExact(position, normal, offset),
                    Math::add(position, Math::multiply(normal, offset))
                );
            }
        }
    }

    const std::array<float, 8> clamp_values = {{
        -2.0f,
        -0.0f,
        0.0f,
        0.37f,
        1.0f,
        4.0f,
        std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN(),
    }};

    for (const float value : clamp_values)
    {
        const float expected = Math::clamp(value, 0.0f, 1.0f);
        const float actual = aoClampExact(value, 0.0f, 1.0f);

        if (std::isnan(expected))
        {
            assert(std::isnan(actual));
        }
        else
        {
            assert(actual == expected);
        }
    }

    assert(aoRadianceIsExactlyZero({0.0f, -0.0f, 0.0f}));
    assert(!aoRadianceIsExactlyZero({0.0f, 0.000001f, 0.0f}));

    return 0;
}
