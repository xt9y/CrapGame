#include "Renderer/Lumen/DistanceFieldSamplePolicy.hpp"
#include "Renderer/Math/Math.hpp"

#include <cassert>
#include <cmath>
#include <limits>

int main()
{
    using namespace Renderer;
    using namespace Renderer::Lumen;

    const float values[] = {
        -3.0f,
        0.0f,
        0.42f,
        1.0f,
        5.0f,
        std::numeric_limits<float>::quiet_NaN(),
    };

    for (const float value : values)
    {
        const float expected = Math::clamp(value, 0.0f, 1.0f);
        const float actual = distanceFieldClampExact(value, 0.0f, 1.0f);

        if (std::isnan(expected))
        {
            assert(std::isnan(actual));
        }
        else
        {
            assert(actual == expected);
        }
    }

    return 0;
}
