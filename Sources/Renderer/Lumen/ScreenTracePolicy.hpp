#ifndef CRAPGAME_RENDERER_LUMEN_SCREEN_TRACE_POLICY_HPP
#define CRAPGAME_RENDERER_LUMEN_SCREEN_TRACE_POLICY_HPP

#include "Renderer/Math/Math.hpp"

namespace Renderer
{
namespace Lumen
{

inline bool screenTraceWithinThicknessSquared(
            const Math::Vec3& delta,
            float thickness
    )
{
    if (thickness <= 0.0f)
    {
        return false;
    }

    return Math::lengthSquared(delta) <= thickness * thickness;
}

} // namespace Lumen
} // namespace Renderer

#endif
