#include "ScreenProbe.hpp"

#include "Renderer/Lumen/Sampling.hpp"

#include <algorithm>

namespace Renderer 
{
namespace Lumen 
{

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

    const std::size_t pixel_count =
        static_cast<std::size_t>(gbuffer.width()) *
        static_cast<std::size_t>(gbuffer.height());

    output->assign(pixel_count, {0.0f, 0.0f, 0.0f});

    for (int tile_y = 0; tile_y < gbuffer.height(); tile_y += probe_spacing) 
    {
        for (int tile_x = 0; tile_x < gbuffer.width(); tile_x += probe_spacing) 
        {
            const int sample_x = std::min(
                    gbuffer.width() - 1,
                    tile_x + probe_spacing / 2
                );

            const int sample_y = std::min(
                    gbuffer.height() - 1,
                    tile_y + probe_spacing / 2
                );

            const GBuffer::Pixel& pixel =
                gbuffer.pixel(sample_x, sample_y);

            if (!pixel.valid) 
            {
                continue;
            }

            const Math::Vec3 origin =
                Math::add(
                        pixel.world_position,
                        Math::multiply(pixel.normal, 0.12f)
                    );

            Math::Vec3 incoming = {0.0f, 0.0f, 0.0f};

            for (int ray = 0; ray < rays_per_probe; ++ray) 
            {
                const Math::Vec3 direction = sampleHemisphere(
                        pixel.normal,
                        ray,
                        rays_per_probe,
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

                Math::Vec3 radiance =
                    radiance_cache.sample(
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
                    1.0f / static_cast<float>(rays_per_probe)
                );

            const Math::Vec3 indirect =
                Math::multiply(
                        Math::multiply(pixel.albedo, incoming),
                        0.75f
                    );

            const int end_x = std::min(
                    gbuffer.width(),
                    tile_x + probe_spacing
                );

            const int end_y = std::min(
                    gbuffer.height(),
                    tile_y + probe_spacing
                );

            for (int y = tile_y; y < end_y; ++y) 
            {
                for (int x = tile_x; x < end_x; ++x) 
                {
                    if (!gbuffer.pixel(x, y).valid) 
                    {
                        continue;
                    }

                    (*output)[
                        static_cast<std::size_t>(y) *
                        static_cast<std::size_t>(gbuffer.width()) +
                        static_cast<std::size_t>(x)
                    ] = indirect;
                }
            }
        }
    }
}

} // namespace Lumen
} // namespace Renderer
