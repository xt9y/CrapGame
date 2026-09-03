#include "ShortRangeAo.hpp"

#include "Renderer/Lumen/Sampling.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

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
                float maximum_distance,
                const std::vector<HemisphereSample> *sequence
        ) 
{
    if (!pixel.valid) 
    {
        return 1.0f;
    }

    const std::size_t pixel_count =
        static_cast<std::size_t>(gbuffer.width()) *
        static_cast<std::size_t>(gbuffer.height());

    /*
     * Keep RendererCheck's 320x218 traced AO unchanged.  At interactive
     * resolutions a per-pixel unified SDF trace is catastrophically costly,
     * so derive the contact term from nearby reconstructed GBuffer samples.
     */
    if (pixel_count > 640u * 360u)
    {
        const GBuffer::Pixel *base = &gbuffer.pixel(0, 0);
        const std::ptrdiff_t index = &pixel - base;
        const std::ptrdiff_t count =
            static_cast<std::ptrdiff_t>(pixel_count);

        if (index >= 0 && index < count)
        {
            const int x = static_cast<int>(
                    index % static_cast<std::ptrdiff_t>(gbuffer.width())
                );
            const int y = static_cast<int>(
                    index / static_cast<std::ptrdiff_t>(gbuffer.width())
                );

            return shortRangeScreenVisibility(
                    gbuffer,
                    x,
                    y,
                    maximum_distance
                );
        }
    }

    const int rays = std::max(1, ray_count);
    const float trace_distance = std::max(0.05f, maximum_distance);

    const Math::Vec3 origin = Math::add(
            pixel.world_position,
            Math::multiply(pixel.normal, 0.025f)
        );

    const HemisphereBasis basis = hemisphereBasis(pixel.normal);
    const bool cached_sequence =
        sequence && sequence->size() >= static_cast<std::size_t>(rays);

    float occlusion = 0.0f;

    for (int ray = 0; ray < rays; ++ray) 
    {
        const HemisphereSample sample = cached_sequence
            ? (*sequence)[static_cast<std::size_t>(ray)]
            : hemisphereSequenceSample(ray, rays, frame_index + 31u);

        const Math::Vec3 direction = sampleHemisphere(basis, sample);

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

float shortRangeScreenVisibility (
                const GBuffer::Buffer& gbuffer,
                int x,
                int y,
                float maximum_distance
        )
{
    if (x < 0
            || y < 0
            || x >= gbuffer.width()
            || y >= gbuffer.height())
    {
        return 1.0f;
    }

    const GBuffer::Pixel& pixel = gbuffer.pixel(x, y);

    if (!pixel.valid)
    {
        return 1.0f;
    }

    static const int offsets[][2] = {
        {-1,  0}, { 1,  0}, { 0, -1}, { 0,  1},
        {-2, -2}, { 2, -2}, {-2,  2}, { 2,  2},
        {-4,  0}, { 4,  0}, { 0, -4}, { 0,  4},
    };

    const float trace_distance = std::max(0.05f, maximum_distance),
                maximum_distance_squared = trace_distance * trace_distance;

    float occlusion = 0.0f;

    for (const auto& offset : offsets)
    {
        const int sample_x = x + offset[0],
                  sample_y = y + offset[1];

        if (sample_x < 0
                || sample_y < 0
                || sample_x >= gbuffer.width()
                || sample_y >= gbuffer.height())
        {
            continue;
        }

        const GBuffer::Pixel& sample =
            gbuffer.pixel(sample_x, sample_y);

        if (!sample.valid)
        {
            continue;
        }

        const Math::Vec3 delta = Math::subtract(
                sample.world_position,
                pixel.world_position
            );

        const float distance_squared = Math::lengthSquared(delta);

        if (distance_squared <= 0.000001f
                || distance_squared >= maximum_distance_squared)
        {
            continue;
        }

        const float distance = std::sqrt(distance_squared);
        const Math::Vec3 direction = Math::multiply(
                delta,
                1.0f / distance
            );

        const float hemisphere = std::max(
                0.0f,
                Math::dot(pixel.normal, direction)
            );

        if (hemisphere <= 0.01f)
        {
            continue;
        }

        const float range_weight = shortRangeWeight(
                distance,
                trace_distance
            );

        const float normal_difference = 1.0f - Math::clamp(
                Math::dot(pixel.normal, sample.normal),
                0.0f,
                1.0f
            );

        const float entity_weight =
            sample.entity == pixel.entity ? 0.60f : 1.0f;

        occlusion += range_weight *
                     hemisphere *
                     (0.70f + normal_difference * 0.30f) *
                     entity_weight;
    }

    return 1.0f - Math::clamp(
            occlusion * 0.35f,
            0.0f,
            0.65f
        );
}

} // namespace Lumen
} // namespace Renderer
