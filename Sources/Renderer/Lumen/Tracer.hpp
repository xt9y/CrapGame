#ifndef CRAPGAME_RENDERER_LUMEN_TRACER_HPP
#define CRAPGAME_RENDERER_LUMEN_TRACER_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/GBuffer/GBuffer.hpp"
#include "Renderer/Lumen/GlobalDistanceField.hpp"
#include "Renderer/Lumen/ScreenTrace.hpp"
#include "Renderer/Lumen/SphereTrace.hpp"

namespace Renderer 
{
namespace Lumen 
{

enum class TraceSource 
{
    None,
    Screen,
    DistanceField,
};

struct UnifiedTraceHit 
{
    Math::Vec3 position,
               normal;

    Ecs::Entity entity = Ecs::INVALID_ENTITY;

    float distance = 0.0f;
    TraceSource source = TraceSource::None;
    bool hit = false;
};

class Tracer 
{

public:
    void build (
                const Ecs::World& world,
                const Math::Vec3& camera_position
        );

    UnifiedTraceHit trace (
                const GBuffer::Buffer& gbuffer,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Math::Vec3& origin,
                const Math::Vec3& direction,
                float maximum_distance
        ) const;

    const DistanceFieldScene& distanceFieldScene () const;
    const GlobalDistanceField& globalDistanceField () const;

private:
    DistanceFieldScene distance_field_scene_;
    GlobalDistanceField global_distance_field_;
};

} // namespace Lumen
} // namespace Renderer

#endif
