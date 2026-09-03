#include "Renderer/Lumen/TraceDirection.hpp"

#include <cassert>
#include <cmath>

int main()
{
    const Renderer::Math::Vec3 direction =
        Renderer::Lumen::normalizedTraceDirection({2.0f, 0.0f, 0.0f});

    assert(std::fabs(direction.x - 1.0f) <= 0.000001f);
    assert(std::fabs(direction.y) <= 0.000001f);
    assert(std::fabs(direction.z) <= 0.000001f);
    return 0;
}
