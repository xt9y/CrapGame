#ifndef CRAPGAME_RENDERER_LUMEN_TRACE_DIRECTION_HPP
#define CRAPGAME_RENDERER_LUMEN_TRACE_DIRECTION_HPP

#include "Renderer/Math/Math.hpp"

namespace Renderer
{
namespace Lumen
{

inline Math::Vec3 normalizedTraceDirection(const Math::Vec3& direction)
{
    return Math::normalize(direction);
}

} // namespace Lumen
} // namespace Renderer

#endif
