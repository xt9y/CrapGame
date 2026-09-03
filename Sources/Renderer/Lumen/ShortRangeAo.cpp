#include "ShortRangeAo.hpp"

#include "Renderer/Lumen/AoSamplingMath.hpp"
#include "Renderer/Lumen/Sampling.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace Renderer 
{
namespace Lumen 
{
namespace
{

template <typename SampleProvider>
float traceShortRangeVisibilityExact(
                const GBuffer::Buffer& gbuffer,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Tracer& tracer,
                const GBuffer::Pixel& pixel,
                int rays,
                float trace_distance,
                SampleProvider&& sample_for_ray
        )
{
    const Math::Vec3 origin = aoOffsetOriginExact(
            pixel.world_position,
            pixel.normal,
            0.025f
        );

    const HemisphereBasis basis = aoHemisphereBasisExact(pixel.normal);
    float occlusion = 0.0f;

    for (int ray = 0; ray < rays; ++ray)
    {
        const HemisphereSample sample = sample_for_ray(ray);
        const Math::Vec3 direction = aoSampleHemisphereExact(basis, sample);

        const VisibilityTraceHit hit = tracer.traceVisibility(
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

    return aoClampExact(
            1.0f - occlusion / static_cast<float>(rays),
            0.0f,
            1.0f
        );
}

} // namespace

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

    float normalized = distance / maximum_distance;

    if (normalized < 0.0f)
    {
        normalized = 0.0f;
    }
    else if (normalized > 1.0f)
    {
        normalized = 1.0f;
    }

    const float remaining = 1.0f - normalized;
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

    const std::size_t pixel_count = gbuffer.pixelCount();

    if (pixel_count > 640u * 360u)
    {
        const GBuffer::Pixel *base = gbuffer.data();
        const std::ptrdiff_t index = &pixel - base;
        const std::ptrdiff_t count =
            static_cast<std::ptrdiff_t>(pixel_count);

        if (index >= 0 && index < count)
        {
            const int width = gbuffer.width();
            const int x = static_cast<int>(
                    index % static_cast<std::ptrdiff_t>(width)
                );
            const int y = static_cast<int>(
                    index / static_cast<std::ptrdiff_t>(width)
                );

            return shortRangeScreenVisibility(
                    gbuffer,
                    x,
                    y,
                    maximum_distance
                );
        }
    }

    if (ray_count == 4
            && maximum_distance == 0.80f
            && sequence
            && sequence->size() >= 4u)
    {
        const HemisphereSample *cached = sequence->data();
        return traceShortRangeVisibilityExact(
                gbuffer,
                view,
                projection,
                tracer,
                pixel,
                4,
                0.80f,
                [cached] (int ray)
                {
                    return cached[static_cast<std::size_t>(ray)];
                }
            );
    }

    const int rays = std::max(1, ray_count);
    const float trace_distance = std::max(0.05f, maximum_distance);
    const bool cached_sequence =
        sequence && sequence->size() >= static_cast<std::size_t>(rays);

    if (cached_sequence)
    {
        const HemisphereSample *cached = sequence->data();
        return traceShortRangeVisibilityExact(
                gbuffer,
                view,
                projection,
                tracer,
                pixel,
                rays,
                trace_distance,
                [cached] (int ray)
                {
                    return cached[static_cast<std::size_t>(ray)];
                }
            );
    }

    return traceShortRangeVisibilityExact(
            gbuffer,
            view,
            projection,
            tracer,
            pixel,
            rays,
            trace_distance,
            [rays, frame_index] (int ray)
            {
                return hemisphereSequenceSample(
                        ray,
                        rays,
                        frame_index + 31u
                    );
            }
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
