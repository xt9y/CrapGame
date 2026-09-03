#include "Renderer/GBuffer/GBuffer.hpp"
#include "Renderer/Lumen/DistanceFieldSamplePolicy.hpp"
#include "Renderer/Lumen/GlobalDistanceFieldPolicy.hpp"
#include "Renderer/Lumen/SphereTrace.hpp"

#include <cassert>
#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>

int main()
{
    using namespace Renderer;

    static_assert(std::is_same_v<
        decltype(std::declval<const GBuffer::Buffer&>().data()),
        const GBuffer::Pixel *
    >);
    static_assert(std::is_same_v<
        decltype(std::declval<GBuffer::Buffer&>().data()),
        GBuffer::Pixel *
    >);
    static_assert(std::is_member_function_pointer_v<
        decltype(&Lumen::DistanceFieldScene::traceNormalized)
    >);
    static_assert(std::is_member_function_pointer_v<
        decltype(&Lumen::DistanceFieldScene::traceDistanceNormalized)
    >);

    const float values[] = {
        -2.0f,
        0.0f,
        0.37f,
        1.0f,
        4.0f,
        std::numeric_limits<float>::quiet_NaN(),
    };

    for (const float value : values)
    {
        const float expected = value < 0.0f
            ? 0.0f
            : (value > 1.0f ? 1.0f : value);
        const float gdf = Lumen::gdfClampExact(value, 0.0f, 1.0f);
        const float sdf = Lumen::distanceFieldClampExact(value, 0.0f, 1.0f);

        if (std::isnan(value))
        {
            assert(std::isnan(gdf));
            assert(std::isnan(sdf));
        }
        else
        {
            assert(gdf == expected);
            assert(sdf == expected);
        }
    }

    return 0;
}
