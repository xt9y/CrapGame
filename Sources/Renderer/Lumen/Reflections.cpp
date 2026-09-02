#include "Reflections.hpp"

#include "Renderer/Lighting/Lighting.hpp"

namespace Renderer 
{
namespace Lumen 
{

void ReflectionSystem::render (
                const GBuffer::Buffer& gbuffer,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Tracer& tracer,
                const SurfaceCache& surface_cache,
                const RadianceCache& radiance_cache,
                const Math::Vec3& camera_position,
                std::uint64_t frame_index,
                std::vector<Math::Vec3> *output
        ) const 
{
    if (!output) 
    {
        return;
    }

    (void)view;
    (void)projection;
    (void)tracer;
    (void)surface_cache;
    (void)frame_index;

    const std::size_t pixel_count =
        static_cast<std::size_t>(gbuffer.width()) *
        static_cast<std::size_t>(gbuffer.height());

    output->assign(pixel_count, {0.0f, 0.0f, 0.0f});

    for (int y = 0; y < gbuffer.height(); ++y) 
    {
        for (int x = 0; x < gbuffer.width(); ++x) 
        {
            const GBuffer::Pixel& pixel =
                gbuffer.pixel(x, y);

            if (!pixel.valid
                    || pixel.roughness < 0.35f) 
            {
                continue;
            }

            const Math::Vec3 view_direction = Math::normalize(
                    Math::subtract(
                            camera_position,
                            pixel.world_position
                        )
                );

            const Math::Vec3 reflection_direction = Math::normalize(
                    Math::reflect(
                            Math::multiply(view_direction, -1.0f),
                            pixel.normal
                        )
                );

            const float sample_distance =
                1.5f + pixel.roughness * 4.0f;

            const Math::Vec3 radiance = radiance_cache.sample(
                    Math::add(
                            pixel.world_position,
                            Math::multiply(
                                    reflection_direction,
                                    sample_distance
                                )
                        )
                );

            const Math::Vec3 f0 = Math::mix(
                    {0.04f, 0.04f, 0.04f},
                    pixel.albedo,
                    Math::saturate(pixel.metallic)
                );

            const Math::Vec3 fresnel = Lighting::fresnelSchlick(
                    Math::saturate(
                            Math::dot(
                                    pixel.normal,
                                    view_direction
                                )
                        ),
                    f0
                );

            const float roughness_weight = Math::clamp(
                    1.0f - pixel.roughness * 0.65f,
                    0.15f,
                    1.0f
                );

            (*output)[
                static_cast<std::size_t>(y) *
                static_cast<std::size_t>(gbuffer.width()) +
                static_cast<std::size_t>(x)
            ] = Math::multiply(
                    Math::multiply(radiance, fresnel),
                    roughness_weight
                );
        }
    }
}

} // namespace Lumen
} // namespace Renderer
