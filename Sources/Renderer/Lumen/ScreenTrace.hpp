#ifndef CRAPGAME_RENDERER_LUMEN_SCREENTRACE_HPP
#define CRAPGAME_RENDERER_LUMEN_SCREENTRACE_HPP

#include "Renderer/GBuffer/GBuffer.hpp"
#include "Renderer/Math/Math.hpp"

namespace Renderer 
{
namespace Lumen 
{

struct TraceHit 
{
    Math::Vec3 position,
               normal;

    Ecs::Entity entity = Ecs::INVALID_ENTITY;

    float distance = 0.0f;

    int x = -1,
        y = -1;

    bool hit = false;
};

TraceHit traceScreen (
                const GBuffer::Buffer& gbuffer,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Math::Vec3& origin,
                const Math::Vec3& direction,
                float maximum_distance,
                float step_size,
                float thickness
        );

} // namespace Lumen
} // namespace Renderer

#endif
