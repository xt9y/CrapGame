#include "Renderer/Lumen/GlobalDistanceFieldPolicy.hpp"
#include "Renderer/Lumen/ScreenTracePolicy.hpp"
#include "Renderer/Lumen/SdfBroadphase.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>

namespace
{

bool near (float a, float b)
{
    return std::fabs(a - b) < 0.0001f;
}

float legacyBoundsDistance(
            const Renderer::Lumen::SdfWorldBounds& bounds,
            const Renderer::Math::Vec3& position
    )
{
    const float dx = std::max(
                std::max(bounds.minimum.x - position.x, 0.0f),
                position.x - bounds.maximum.x
            ),
            dy = std::max(
                std::max(bounds.minimum.y - position.y, 0.0f),
                position.y - bounds.maximum.y
            ),
            dz = std::max(
                std::max(bounds.minimum.z - position.z, 0.0f),
                position.z - bounds.maximum.z
            );

    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool legacyContains(
            const Renderer::Math::Vec3& center,
            float half_extent,
            const Renderer::Math::Vec3& position
    )
{
    return std::fabs(position.x - center.x) <= half_extent
        && std::fabs(position.y - center.y) <= half_extent
        && std::fabs(position.z - center.z) <= half_extent;
}

int legacyPixelIndex(float normalized_coordinate, int extent)
{
    return std::max(
            0,
            std::min(
                    extent - 1,
                    static_cast<int>(
                            normalized_coordinate * static_cast<float>(extent)
                        )
                )
        );
}

}

int main ()
{
    using Renderer::Lumen::SdfWorldBounds;
    using Renderer::Lumen::sdfBoundsDistance;

    const SdfWorldBounds bounds = {
        {-1.0f, -2.0f, -3.0f},
        { 1.0f,  2.0f,  3.0f},
    };

    assert(near(sdfBoundsDistance(bounds, {0.0f, 0.0f, 0.0f}), 0.0f));
    assert(near(sdfBoundsDistance(bounds, {3.0f, 0.0f, 0.0f}), 2.0f));
    assert(near(sdfBoundsDistance(bounds, {1.0f, 5.0f, 3.0f}), 3.0f));
    assert(near(sdfBoundsDistance(bounds, {4.0f, 6.0f, 3.0f}), 5.0f));

    const std::array<Renderer::Math::Vec3, 8> bound_positions = {{
        {0.0f, 0.0f, 0.0f},
        {-1.0f, -2.0f, -3.0f},
        {1.0f, 2.0f, 3.0f},
        {-1.1f, 0.0f, 0.0f},
        {1.1f, 2.2f, 3.3f},
        {-7.0f, -8.0f, -9.0f},
        {0.0f, 10.0f, 0.0f},
        {12.0f, 0.5f, -7.0f},
    }};

    for (const Renderer::Math::Vec3& position : bound_positions)
    {
        assert(
            legacyBoundsDistance(bounds, position) ==
            sdfBoundsDistance(bounds, position)
        );
    }

    const Renderer::Math::Vec3 center = {1.25f, -2.5f, 0.75f};
    constexpr float half_extent = 8.0f;
    const std::array<Renderer::Math::Vec3, 5> gdf_positions = {{
        {1.25f, -2.5f, 0.75f},
        {9.25f, -2.5f, 0.75f},
        {-6.75f, -2.5f, 0.75f},
        {9.2501f, -2.5f, 0.75f},
        {0.125f, 1.75f, -3.25f},
    }};

    for (const Renderer::Math::Vec3& position : gdf_positions)
    {
        assert(
            legacyContains(center, half_extent, position) ==
            Renderer::Lumen::gdfContainsExact(center, half_extent, position)
        );
    }

    const std::array<float, 8> coordinates = {{
        0.0f, 0.000001f, 0.125f, 0.499999f,
        0.5f, 0.999f, 0.999999f, 1.0f,
    }};

    for (const int extent : {1, 2, 17, 320, 870, 1280})
    {
        for (const float coordinate : coordinates)
        {
            assert(
                legacyPixelIndex(coordinate, extent) ==
                Renderer::Lumen::screenTraceNormalizedPixelIndexExact(
                        coordinate,
                        extent
                    )
            );
        }
    }

    return 0;
}
