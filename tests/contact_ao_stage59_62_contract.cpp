#include "Renderer/Lumen/ScreenTraceMath.hpp"
#include "Renderer/Lumen/ScreenTracePolicy.hpp"
#include "Renderer/Math/Math.hpp"

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

void assertVec4Exact(
            const Renderer::Math::Vec4& actual,
            const Renderer::Math::Vec4& expected
    )
{
    assert(actual.x == expected.x);
    assert(actual.y == expected.y);
    assert(actual.z == expected.z);
    assert(actual.w == expected.w);
}

} // namespace

int main()
{
    using namespace Renderer;
    using namespace Renderer::Lumen;

    const Math::Vec3 origin = {1.25f, -2.5f, 0.75f};
    const Math::Vec3 direction = {-0.31f, 0.72f, 0.58f};
    const float distance = 0.48f;

    const Math::Vec3 expected_position = Math::add(
            origin,
            Math::multiply(direction, distance)
        );
    assertVec3Exact(
            screenTraceSamplePosition(origin, direction, distance),
            expected_position
        );

    Math::Mat4 matrix = {};
    for (int index = 0; index < 16; ++index)
    {
        matrix.value[index] =
            (static_cast<float>(index) - 7.0f) * 0.137f;
    }

    const Math::Vec4 expected_point = Math::transform(
            matrix,
            {
                expected_position.x,
                expected_position.y,
                expected_position.z,
                1.0f,
            }
        );
    assertVec4Exact(
            screenTraceTransformPoint(matrix, expected_position),
            expected_point
        );

    const Math::Vec4 value = {0.23f, -0.91f, 1.7f, 0.44f};
    assertVec4Exact(
            screenTraceTransform(matrix, value),
            Math::transform(matrix, value)
        );

    const Math::Vec3 thickness_delta = {0.11f, -0.07f, 0.04f};
    const float thickness = 0.15f;
    assert(
        screenTraceWithinThicknessSquared(thickness_delta, thickness)
        == (Math::lengthSquared(thickness_delta) <= thickness * thickness)
    );

    return 0;
}
