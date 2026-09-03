#include "Tracer.hpp"

#include "Renderer/Lumen/SdfTraceMath.hpp"
#include "Renderer/Lumen/TraceDirection.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer 
{
namespace Lumen 
{

void Tracer::build (
                const Ecs::World& world,
                const Math::Vec3& camera_position
        ) 
{
    distance_field_scene_.build(world);
    global_distance_field_.build(
            distance_field_scene_,
            camera_position
        );
}

UnifiedTraceHit Tracer::trace (
                const GBuffer::Buffer& gbuffer,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Math::Vec3& origin,
                const Math::Vec3& direction,
                float maximum_distance
        ) const 
{
    UnifiedTraceHit result;

    if (maximum_distance <= 0.0f) 
    {
        return result;
    }

    const Math::Vec3 ray_direction =
        normalizedTraceDirection(direction);

    const TraceHit screen_hit = traceScreenNormalized(
            gbuffer,
            view,
            projection,
            origin,
            ray_direction,
            maximum_distance,
            0.12f,
            0.18f
        );

    if (screen_hit.hit) 
    {
        result.position = screen_hit.position;
        result.normal = screen_hit.normal;
        result.entity = screen_hit.entity;
        result.distance = screen_hit.distance;
        result.source = TraceSource::Screen;
        result.hit = true;
        return result;
    }

    const Math::Vec3 sdf_direction =
        normalizedTraceDirection(ray_direction);
    float travelled = 0.05f;

    for (int step = 0;
            step < 96 && travelled <= maximum_distance;
            ++step) 
    {
        const Math::Vec3 position =
            sdfTraceSamplePositionExact(
                    origin,
                    ray_direction,
                    travelled
                );

        const float global_distance =
            global_distance_field_.sample(position);

        if (!std::isfinite(global_distance)) 
        {
            break;
        }

        if (global_distance <= 0.18f) 
        {
            const float remaining =
                maximum_distance - travelled;

            const SdfHit sdf_hit =
                distance_field_scene_.traceNormalized(
                        position,
                        sdf_direction,
                        std::min(2.0f, remaining),
                        64,
                        0.01f,
                        0.035f
                    );

            if (sdf_hit.hit) 
            {
                result.position = sdf_hit.position;
                result.normal = sdf_hit.normal;
                result.entity = sdf_hit.entity;
                result.distance = travelled + sdf_hit.distance;
                result.source = TraceSource::DistanceField;
                result.hit = true;
                return result;
            }
        }

        travelled += std::max(0.04f, global_distance * 0.75f);
    }

    const SdfHit fallback =
        distance_field_scene_.traceNormalized(
                origin,
                sdf_direction,
                maximum_distance
            );

    if (fallback.hit) 
    {
        result.position = fallback.position;
        result.normal = fallback.normal;
        result.entity = fallback.entity;
        result.distance = fallback.distance;
        result.source = TraceSource::DistanceField;
        result.hit = true;
    }

    return result;
}

VisibilityTraceHit Tracer::traceVisibility (
                const GBuffer::Buffer& gbuffer,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Math::Vec3& origin,
                const Math::Vec3& direction,
                float maximum_distance
        ) const
{
    VisibilityTraceHit result;

    if (maximum_distance <= 0.0f)
    {
        return result;
    }

    const Math::Vec3 ray_direction = normalizedTraceDirection(direction);
    const ScreenDistanceHit screen_hit = traceScreenDistanceNormalized(
            gbuffer,
            view,
            projection,
            origin,
            ray_direction,
            maximum_distance,
            0.12f,
            0.18f
        );

    if (screen_hit.hit)
    {
        result.distance = screen_hit.distance;
        result.hit = true;
        return result;
    }

    const Math::Vec3 sdf_direction =
        normalizedTraceDirection(ray_direction);
    float travelled = 0.05f;

    for (int step = 0;
            step < 96 && travelled <= maximum_distance;
            ++step)
    {
        const Math::Vec3 position = sdfTraceSamplePositionExact(
                origin,
                ray_direction,
                travelled
            );

        const float global_distance = global_distance_field_.sample(position);

        if (!std::isfinite(global_distance))
        {
            break;
        }

        if (global_distance <= 0.18f)
        {
            const float remaining = maximum_distance - travelled;
            const SdfDistanceHit sdf_hit =
                distance_field_scene_.traceDistanceNormalized(
                        position,
                        sdf_direction,
                        std::min(2.0f, remaining),
                        64,
                        0.01f,
                        0.035f
                    );

            if (sdf_hit.hit)
            {
                result.distance = travelled + sdf_hit.distance;
                result.hit = true;
                return result;
            }
        }

        travelled += std::max(0.04f, global_distance * 0.75f);
    }

    const SdfDistanceHit fallback =
        distance_field_scene_.traceDistanceNormalized(
                origin,
                sdf_direction,
                maximum_distance
            );

    if (fallback.hit)
    {
        result.distance = fallback.distance;
        result.hit = true;
    }

    return result;
}

const DistanceFieldScene& Tracer::distanceFieldScene () const 
{
    return distance_field_scene_;
}

const GlobalDistanceField& Tracer::globalDistanceField () const 
{
    return global_distance_field_;
}

} // namespace Lumen
} // namespace Renderer
