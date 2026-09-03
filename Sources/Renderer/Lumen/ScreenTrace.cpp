#include "ScreenTrace.hpp"

#include "Renderer/Lumen/ScreenTraceMath.hpp"
#include "Renderer/Lumen/ScreenTracePolicy.hpp"
#include "Renderer/Lumen/TraceDirection.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer 
{
namespace Lumen 
{
namespace 
{

bool projectPoint (
                const Math::Vec3& position,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                float *screen_x,
                float *screen_y
        ) 
{
    const Math::Vec4 view_position =
        screenTraceTransformPoint(view, position);

    const Math::Vec4 clip_position =
        screenTraceTransform(projection, view_position);

    if (clip_position.w <= 0.00001f) 
    {
        return false;
    }

    const float inverse_w = 1.0f / clip_position.w,
                ndc_x = clip_position.x * inverse_w,
                ndc_y = clip_position.y * inverse_w,
                ndc_z = clip_position.z * inverse_w;

    if (ndc_x < -1.0f || ndc_x > 1.0f
            || ndc_y < -1.0f || ndc_y > 1.0f
            || ndc_z < -1.0f || ndc_z > 1.0f) 
    {
        return false;
    }

    if (screen_x) 
    {
        *screen_x = ndc_x * 0.5f + 0.5f;
    }

    if (screen_y) 
    {
        *screen_y = 1.0f - (ndc_y * 0.5f + 0.5f);
    }

    return true;
}

} // namespace

TraceHit traceScreenNormalized (
                const GBuffer::Buffer& gbuffer,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Math::Vec3& origin,
                const Math::Vec3& normalized_direction,
                float maximum_distance,
                float step_size,
                float thickness
        ) 
{
    TraceHit result;

    const int width = gbuffer.width(),
              height = gbuffer.height();

    if (width <= 0
            || height <= 0
            || maximum_distance <= 0.0f
            || step_size <= 0.0f
            || thickness <= 0.0f) 
    {
        return result;
    }

    const float width_float = static_cast<float>(width),
                height_float = static_cast<float>(height);

    const GBuffer::Pixel *pixels = gbuffer.data();

    for (float distance = step_size;
            distance <= maximum_distance;
            distance += step_size) 
    {
        const Math::Vec3 sample_position =
            screenTraceSamplePosition(
                    origin,
                    normalized_direction,
                    distance
                );

        float screen_x = 0.0f,
              screen_y = 0.0f;

        if (!projectPoint(
                sample_position,
                view,
                projection,
                &screen_x,
                &screen_y
            )) 
        {
            continue;
        }

        const int x = std::max(
                0,
                std::min(
                        width - 1,
                        static_cast<int>(screen_x * width_float)
                    )
            );

        const int y = std::max(
                0,
                std::min(
                        height - 1,
                        static_cast<int>(screen_y * height_float)
                    )
            );

        const GBuffer::Pixel& pixel = pixels[
            static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
            static_cast<std::size_t>(x)
        ];

        if (!pixel.valid) 
        {
            continue;
        }

        const Math::Vec3 surface_delta = {
            sample_position.x - pixel.world_position.x,
            sample_position.y - pixel.world_position.y,
            sample_position.z - pixel.world_position.z,
        };

        if (!screenTraceWithinThicknessSquared(surface_delta, thickness))
        {
            continue;
        }

        result.position = pixel.world_position;
        result.normal = pixel.normal;
        result.entity = pixel.entity;
        result.distance = distance;
        result.x = x;
        result.y = y;
        result.hit = true;
        return result;
    }

    return result;
}

TraceHit traceScreen (
                const GBuffer::Buffer& gbuffer,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Math::Vec3& origin,
                const Math::Vec3& direction,
                float maximum_distance,
                float step_size,
                float thickness
        ) 
{
    return traceScreenNormalized(
            gbuffer,
            view,
            projection,
            origin,
            normalizedTraceDirection(direction),
            maximum_distance,
            step_size,
            thickness
        );
}

} // namespace Lumen
} // namespace Renderer
