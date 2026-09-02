#include "ScreenProbe.hpp"

#include "Renderer/Lumen/Sampling.hpp"
#include "Renderer/Lumen/ShortRangeAo.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Renderer 
{
namespace Lumen 
{
namespace 
{

struct ProbeSample 
{
    Math::Vec3 position,
               normal,
               indirect;

    Ecs::Entity entity = Ecs::INVALID_ENTITY;

    int x = 0,
        y = 0;

    bool valid = false;
};

std::size_t probeIndex (
                int x,
                int y,
                int width
        ) 
{
    return static_cast<std::size_t>(y) *
           static_cast<std::size_t>(width) +
           static_cast<std::size_t>(x);
}

bool findProbePixel (
                const GBuffer::Buffer& gbuffer,
                int tile_x,
                int tile_y,
                int probe_spacing,
                int *sample_x,
                int *sample_y
        ) 
{
    if (!sample_x
            || !sample_y) 
    {
        return false;
    }

    const int end_x = std::min(
            gbuffer.width(),
            tile_x + probe_spacing
        );

    const int end_y = std::min(
            gbuffer.height(),
            tile_y + probe_spacing
        );

    const int center_x = std::min(
            gbuffer.width() - 1,
            tile_x + probe_spacing / 2
        );

    const int center_y = std::min(
            gbuffer.height() - 1,
            tile_y + probe_spacing / 2
        );

    int best_distance = std::numeric_limits<int>::max();
    bool found = false;

    for (int y = tile_y; y < end_y; ++y) 
    {
        for (int x = tile_x; x < end_x; ++x) 
        {
            if (!gbuffer.pixel(x, y).valid) 
            {
                continue;
            }

            const int difference_x = x - center_x,
                      difference_y = y - center_y;

            const int distance = difference_x * difference_x +
                                 difference_y * difference_y;

            if (distance >= best_distance) 
            {
                continue;
            }

            best_distance = distance;
            *sample_x = x;
            *sample_y = y;
            found = true;
        }
    }

    return found;
}

Math::Vec3 gatherProbe (
                const GBuffer::Buffer& gbuffer,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Tracer& tracer,
                const SurfaceCache& surface_cache,
                const RadianceCache& radiance_cache,
                const GBuffer::Pixel& pixel,
                std::uint64_t frame_index,
                int ray_count
        ) 
{
    const Math::Vec3 origin = Math::add(
            pixel.world_position,
            Math::multiply(pixel.normal, 0.04f)
        );

    Math::Vec3 incoming = {0.0f, 0.0f, 0.0f};

    for (int ray = 0; ray < ray_count; ++ray) 
    {
        const Math::Vec3 direction = sampleHemisphere(
                pixel.normal,
                ray,
                ray_count,
                frame_index
            );

        const UnifiedTraceHit hit = tracer.trace(
                gbuffer,
                view,
                projection,
                origin,
                direction,
                24.0f
            );

        Math::Vec3 radiance = radiance_cache.sample(
                Math::add(
                        origin,
                        Math::multiply(direction, 4.0f)
                    )
            );

        if (hit.hit) 
        {
            radiance = surface_cache.radiance(
                    hit.entity,
                    hit.position,
                    hit.normal
                );
        }

        incoming = Math::add(incoming, radiance);
    }

    incoming = Math::multiply(
            incoming,
            1.0f / static_cast<float>(ray_count)
        );

    return Math::multiply(
            Math::multiply(pixel.albedo, incoming),
            0.35f
        );
}

} // namespace

void ScreenProbeGather::gather (
                const GBuffer::Buffer& gbuffer,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Tracer& tracer,
                const SurfaceCache& surface_cache,
                const RadianceCache& radiance_cache,
                std::uint64_t frame_index,
                int spacing,
                int ray_count,
                std::vector<Math::Vec3> *output
        ) const 
{
    if (!output) 
    {
        return;
    }

    const int probe_spacing = std::max(4, spacing),
              rays_per_probe = std::max(1, ray_count);

    const int probe_width =
        (gbuffer.width() + probe_spacing - 1) / probe_spacing;

    const int probe_height =
        (gbuffer.height() + probe_spacing - 1) / probe_spacing;

    const std::size_t pixel_count =
        static_cast<std::size_t>(gbuffer.width()) *
        static_cast<std::size_t>(gbuffer.height());

    std::vector<ProbeSample> probes(
            static_cast<std::size_t>(probe_width) *
            static_cast<std::size_t>(probe_height)
        );

    output->assign(pixel_count, {0.0f, 0.0f, 0.0f});

    for (int probe_y = 0; probe_y < probe_height; ++probe_y) 
    {
        for (int probe_x = 0; probe_x < probe_width; ++probe_x) 
        {
            const int tile_x = probe_x * probe_spacing,
                      tile_y = probe_y * probe_spacing;

            int sample_x = 0,
                sample_y = 0;

            if (!findProbePixel(
                    gbuffer,
                    tile_x,
                    tile_y,
                    probe_spacing,
                    &sample_x,
                    &sample_y
                )) 
            {
                continue;
            }

            const GBuffer::Pixel& pixel =
                gbuffer.pixel(sample_x, sample_y);

            ProbeSample& probe =
                probes[probeIndex(probe_x, probe_y, probe_width)];

            probe.position = pixel.world_position;
            probe.normal = pixel.normal;
            probe.indirect = gatherProbe(
                    gbuffer,
                    view,
                    projection,
                    tracer,
                    surface_cache,
                    radiance_cache,
                    pixel,
                    frame_index,
                    rays_per_probe
                );
            probe.entity = pixel.entity;
            probe.x = sample_x;
            probe.y = sample_y;
            probe.valid = true;
        }
    }

    std::vector<Math::Vec3> reconstructed(
            pixel_count,
            {0.0f, 0.0f, 0.0f}
        );

    for (int y = 0; y < gbuffer.height(); ++y) 
    {
        for (int x = 0; x < gbuffer.width(); ++x) 
        {
            const GBuffer::Pixel& pixel =
                gbuffer.pixel(x, y);

            if (!pixel.valid) 
            {
                continue;
            }

            const int center_probe_x = std::max(
                    0,
                    std::min(
                            probe_width - 1,
                            x / probe_spacing
                        )
                );

            const int center_probe_y = std::max(
                    0,
                    std::min(
                            probe_height - 1,
                            y / probe_spacing
                        )
                );

            Math::Vec3 indirect = {0.0f, 0.0f, 0.0f};
            float total_weight = 0.0f;

            for (int offset_y = -1; offset_y <= 1; ++offset_y) 
            {
                for (int offset_x = -1; offset_x <= 1; ++offset_x) 
                {
                    const int probe_x = center_probe_x + offset_x,
                              probe_y = center_probe_y + offset_y;

                    if (probe_x < 0
                            || probe_x >= probe_width
                            || probe_y < 0
                            || probe_y >= probe_height) 
                    {
                        continue;
                    }

                    const ProbeSample& probe = probes[
                        probeIndex(probe_x, probe_y, probe_width)
                    ];

                    if (!probe.valid
                            || probe.entity != pixel.entity) 
                    {
                        continue;
                    }

                    const float normal_similarity = Math::dot(
                            pixel.normal,
                            probe.normal
                        );

                    if (normal_similarity < 0.65f) 
                    {
                        continue;
                    }

                    const float screen_x =
                        static_cast<float>(x - probe.x) /
                        static_cast<float>(probe_spacing);

                    const float screen_y =
                        static_cast<float>(y - probe.y) /
                        static_cast<float>(probe_spacing);

                    const float screen_distance = screen_x * screen_x +
                                                  screen_y * screen_y;

                    const float world_distance = Math::lengthSquared(
                            Math::subtract(
                                    pixel.world_position,
                                    probe.position
                                )
                        );

                    const float normal_weight =
                        std::pow(normal_similarity, 8.0f);

                    const float weight = normal_weight /
                        (
                            0.20f +
                            screen_distance +
                            world_distance * 1.5f
                        );

                    indirect = Math::add(
                            indirect,
                            Math::multiply(probe.indirect, weight)
                        );

                    total_weight += weight;
                }
            }

            if (total_weight > 0.00001f) 
            {
                indirect = Math::multiply(
                        indirect,
                        1.0f / total_weight
                    );
            }
            else 
            {
                indirect = Math::multiply(
                        Math::multiply(
                                pixel.albedo,
                                radiance_cache.sample(
                                        pixel.world_position
                                    )
                            ),
                        0.20f
                    );
            }

            reconstructed[
                static_cast<std::size_t>(y) *
                static_cast<std::size_t>(gbuffer.width()) +
                static_cast<std::size_t>(x)
            ] = indirect;
        }
    }

    for (int y = 0; y < gbuffer.height(); ++y) 
    {
        for (int x = 0; x < gbuffer.width(); ++x) 
        {
            const GBuffer::Pixel& pixel =
                gbuffer.pixel(x, y);

            if (!pixel.valid) 
            {
                continue;
            }

            Math::Vec3 filtered = {0.0f, 0.0f, 0.0f};
            float total_weight = 0.0f;

            for (int offset_y = -1; offset_y <= 1; ++offset_y) 
            {
                for (int offset_x = -1; offset_x <= 1; ++offset_x) 
                {
                    const int sample_x = x + offset_x,
                              sample_y = y + offset_y;

                    if (sample_x < 0
                            || sample_x >= gbuffer.width()
                            || sample_y < 0
                            || sample_y >= gbuffer.height()) 
                    {
                        continue;
                    }

                    const GBuffer::Pixel& sample =
                        gbuffer.pixel(sample_x, sample_y);

                    if (!sample.valid
                            || sample.entity != pixel.entity) 
                    {
                        continue;
                    }

                    const float normal_similarity = Math::dot(
                            pixel.normal,
                            sample.normal
                        );

                    if (normal_similarity < 0.80f) 
                    {
                        continue;
                    }

                    const float world_distance = Math::lengthSquared(
                            Math::subtract(
                                    pixel.world_position,
                                    sample.world_position
                                )
                        );

                    const float weight =
                        std::pow(normal_similarity, 12.0f) /
                        (1.0f + world_distance * 8.0f);

                    filtered = Math::add(
                            filtered,
                            Math::multiply(
                                    reconstructed[
                                        static_cast<std::size_t>(sample_y) *
                                        static_cast<std::size_t>(gbuffer.width()) +
                                        static_cast<std::size_t>(sample_x)
                                    ],
                                    weight
                                )
                        );

                    total_weight += weight;
                }
            }

            if (total_weight <= 0.00001f) 
            {
                continue;
            }

            const Math::Vec3 indirect = Math::multiply(
                    filtered,
                    1.0f / total_weight
                );

            const float visibility = shortRangeVisibility(
                    gbuffer,
                    view,
                    projection,
                    tracer,
                    pixel,
                    frame_index,
                    4,
                    0.80f
                );

            const float contact_weight =
                0.35f + visibility * 0.65f;

            (*output)[
                static_cast<std::size_t>(y) *
                static_cast<std::size_t>(gbuffer.width()) +
                static_cast<std::size_t>(x)
            ] = Math::multiply(
                    indirect,
                    contact_weight
                );
        }
    }
}

} // namespace Lumen
} // namespace Renderer
