#include "Renderer/Lumen/SdfBroadphase.hpp"

#include <cassert>
#include <cmath>

namespace
{

bool near (float a, float b)
{
    return std::fabs(a - b) < 0.0001f;
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

    return 0;
}
