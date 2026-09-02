#include "ShortRangeAo.hpp"

#include "Renderer/Lumen/Sampling.hpp"

#include <algorithm>

namespace Renderer 
{
namespace Lumen 
{

float shortRangeWeight (
                float distance,
                float maximum_distance
        ) 
{
    if (maximum_distance <= 0.00001f
            || distance < 0.0f
            || distance >= maximum_distance) 
    {
        return 0.0f;
    }

    const float remaining = 1.0f - Math::clamp(
            distance / maximum_distance,
            0.0f,
            1.0f
        );

    return remaining * remaining;
}

float shortRangeVisibility (
                const GBuffer::Buffer& gbuffer,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Tracer& tracer,
                const GBuffer::Pixel& pixel,
                std::uint64_t frame_index,
                int ray_count,
                float maximum_distance
        ) 
{
    if (!pixel.valid) 
    {
        return 1.0f;
    }

    const int rays = std::max(1, ray_count);
    const float trace_distance = std::max(0.05f, maximum_distance);

    const Math::Vec3 origin = Math::add(
            pixel.world_position,
            Math::multiply(pixel.normal, 0.025f)
        );

    float occlusion = 0.0f;

    for (int ray = 0; ray < rays; ++ray) 
    {
        const Math::Vec3 direction = sampleHemisphere(
                pixel.normal,
                ray,
                rays,
                frame_index + 31u
            );

        const UnifiedTraceHit hit = tracer.trace(
                gbuffer,
                view,
                projection,
                origin,
                direction,
                trace_distance
            );

        if (!hit.hit) 
        {
            continue;
        }

        occlusion += shortRangeWeight(
                hit.distance,
                trace_distance
            );
    }

    return Math::clamp(
            1.0f - occlusion / static_cast<float>(rays),
            0.0f,
            1.0f
        );
}

} // namespace Lumen
} // namespace Renderer
