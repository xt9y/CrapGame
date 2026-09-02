#include "ScreenProbe.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer 
{
namespace Lumen 
{
namespace 
{

constexpr float GOLDEN = 2.39996322972865332f;

Math::Vec3 hemisphereDirection (
                const Math::Vec3& normal,
                int index,
                int count,
                std::uint64_t frame_index
        ) 
{
    const float u = (static_cast<float>(index) + 0.5f) /
                    static_cast<float>(std::max(1, count));

    const float radius = std::sqrt(u),
                angle = GOLDEN *
                        static_cast<float>(index + static_cast<int>(frame_index % 17u)),
                local_x = std::cos(angle) * radius,
                local_z = std::sin(angle) * radius,
                local_y = std::sqrt(std::max(0.0f, 1.0f - u));

    const Math::Vec3 n = Math::normalize(normal);

    const Math::Vec3 reference =
        std::fabs(n.y) < 0.95f
        ? Math::Vec3{0.0f, 1.0f, 0.0f}
        : Math::Vec3{1.0f, 0.0f, 0.0f};

    const Math::Vec3 tangent =
        Math::normalize(Math::cross(reference, n));

    const Math::Vec3 bitangent =
        Math::normalize(Math::cross(n, tangent));

    return Math::normalize(
            Math::add(
                    Math::add(
                            Math::multiply(tangent, local_x),
                            Math::multiply(n, local_y)
                        ),
                    Math::multiply(bitangent, local_z)
                )
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
                const Math::Vec3 direction = hemisphereDirection(
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
