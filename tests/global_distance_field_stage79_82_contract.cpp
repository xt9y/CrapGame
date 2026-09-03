#include "Renderer/Lumen/GlobalDistanceFieldPolicy.hpp"
#include "Renderer/Math/Math.hpp"

#include <cassert>
#include <cmath>
#include <limits>

int main()
{
    using namespace Renderer;
    using namespace Renderer::Lumen;

    const float values[] = {
        -2.0f,
        0.0f,
        0.37f,
        1.0f,
        3.0f,
        std::numeric_limits<float>::quiet_NaN(),
    };

    for (const float value : values)
    {
        const float expected = Math::clamp(value, 0.0f, 1.0f);
        const float actual = gdfClampExact(value, 0.0f, 1.0f);

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
