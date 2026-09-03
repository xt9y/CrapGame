#include "Renderer/Lumen/SdfTraceMath.hpp"
#include "Renderer/Lumen/SdfTransformCache.hpp"
#include "Renderer/Lumen/TraceDirection.hpp"

#include <array>
#include <cassert>
#include <cmath>

namespace
{

void normalizationMatchesMathExactly()
{
    const std::array<Renderer::Math::Vec3, 8> directions = {{
        {2.0f, 0.0f, 0.0f},
        {0.0f, -3.0f, 0.0f},
        {0.0f, 0.0f, 7.0f},
        {1.0f, 2.0f, 3.0f},
        {-4.25f, 0.75f, 9.5f},
        {0.0000001f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {-0.3713907f, 0.2171131f, 0.903f},
    }};

    for (const Renderer::Math::Vec3& input : directions)
    {
        const Renderer::Math::Vec3 reference =
            Renderer::Math::normalize(input);
        const Renderer::Math::Vec3 optimized =
            Renderer::Lumen::normalizedTraceDirection(input);

        assert(reference.x == optimized.x);
        assert(reference.y == optimized.y);
        assert(reference.z == optimized.z);
    }
}

void samplePositionMatchesMathExactly()
{
    const std::array<Renderer::Math::Vec3, 4> origins = {{
        {0.0f, 0.0f, 0.0f},
        {1.25f, -3.5f, 7.75f},
        {-12.0f, 0.125f, 4.0f},
        {0.001f, -0.002f, 0.003f},
    }};

    const std::array<Renderer::Math::Vec3, 4> directions = {{
        {1.0f, 0.0f, 0.0f},
        {-0.25f, 0.5f, 0.75f},
        {0.3713907f, -0.2171131f, 0.903f},
        {-1.0f, -1.0f, -1.0f},
    }};

    const std::array<float, 5> distances = {{
        0.0f,
        0.01f,
        0.05f,
        0.8f,
        17.25f,
    }};

    for (const Renderer::Math::Vec3& origin : origins)
    {
        for (const Renderer::Math::Vec3& direction : directions)
        {
            for (const float distance : distances)
            {
                const Renderer::Math::Vec3 reference = Renderer::Math::add(
                        origin,
                        Renderer::Math::multiply(direction, distance)
                    );
                const Renderer::Math::Vec3 optimized =
                    Renderer::Lumen::sdfTraceSamplePositionExact(
                            origin,
                            direction,
                            distance
                        );

                assert(reference.x == optimized.x);
                assert(reference.y == optimized.y);
                assert(reference.z == optimized.z);
            }
        }
    }
}

Renderer::Math::Vec3 legacyInverseTransformPointCached(
            const Renderer::Math::Vec3& value,
            const Renderer::Lumen::CachedInverseTransform& transform
    )
{
    Renderer::Math::Vec3 result =
        Renderer::Math::subtract(value, transform.position);

    const float z_x = transform.cos_z * result.x - transform.sin_z * result.y,
                z_y = transform.sin_z * result.x + transform.cos_z * result.y;
    result.x = z_x;
    result.y = z_y;

    const float x_y = transform.cos_x * result.y - transform.sin_x * result.z,
                x_z = transform.sin_x * result.y + transform.cos_x * result.z;
    result.y = x_y;
    result.z = x_z;

    const float y_x = transform.cos_y * result.x + transform.sin_y * result.z,
                y_z = -transform.sin_y * result.x + transform.cos_y * result.z;
    result.x = y_x;
    result.z = y_z;

    result.x *= transform.inverse_scale.x;
    result.y *= transform.inverse_scale.y;
    result.z *= transform.inverse_scale.z;
    return result;
}

void inverseTransformMatchesExactly()
{
    const std::array<Ecs::TransformComponent, 4> transforms = {{
        {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        {{2.5f, -1.25f, 7.0f}, {17.0f, 31.0f, 9.0f}, {0.65f, 1.4f, 0.9f}},
        {{-4.0f, 3.0f, 0.5f}, {-30.0f, 75.0f, 12.0f}, {-1.0f, 2.0f, 0.5f}},
        {{0.1f, 0.2f, 0.3f}, {90.0f, -45.0f, 180.0f}, {3.0f, 0.25f, 1.25f}},
    }};

    const std::array<Renderer::Math::Vec3, 5> points = {{
        {0.0f, 0.0f, 0.0f},
        {1.0f, 2.0f, 3.0f},
        {-7.5f, 4.25f, 0.125f},
        {18.0f, -9.0f, 2.0f},
        {0.001f, -0.002f, 0.003f},
    }};

    for (const Ecs::TransformComponent& transform : transforms)
    {
        const Renderer::Lumen::CachedInverseTransform cached =
            Renderer::Lumen::cacheInverseTransform(transform);

        for (const Renderer::Math::Vec3& point : points)
        {
            const Renderer::Math::Vec3 reference =
                legacyInverseTransformPointCached(point, cached);
            const Renderer::Math::Vec3 optimized =
                Renderer::Lumen::inverseTransformPointCached(point, cached);

            assert(reference.x == optimized.x);
            assert(reference.y == optimized.y);
            assert(reference.z == optimized.z);
        }
    }
}

} // namespace

int main()
{
    normalizationMatchesMathExactly();
    samplePositionMatchesMathExactly();
    inverseTransformMatchesExactly();
    return 0;
}
