#include "Reflections.hpp"

#include "Renderer/Lighting/Lighting.hpp"
#include "Renderer/Lumen/ParallelRows.hpp"
#include "Renderer/Lumen/ReflectionPolicy.hpp"

#include <thread>

namespace Renderer 
{
namespace Lumen 
{
namespace 
{

Math::Vec3 reflectionFresnel (
                const GBuffer::Pixel& pixel,
                const Math::Vec3& view_direction
        ) 
{
    const Math::Vec3 f0 = Math::mix(
            {0.04f, 0.04f, 0.04f},
            pixel.albedo,
            Math::saturate(pixel.metallic)
        );

    return Lighting::fresnelSchlick(
            Math::saturate(
                    Math::dot(
                            pixel.normal,
                            view_direction
                        )
                ),
            f0
        );
}

} // namespace

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
        ) 
{
    if (!output) 
    {
        return;
    }

    (void)frame_index;

    const std::size_t pixel_count =
        static_cast<std::size_t>(gbuffer.width()) *
        static_cast<std::size_t>(gbuffer.height());

    if (raw_.size() != pixel_count)
    {
        raw_.resize(pixel_count);
    }

    parallelRowsDynamic(
            gbuffer.height(),
            std::thread::hardware_concurrency(),
            [&](int y)
            {
                for (int x = 0; x < gbuffer.width(); ++x) 
                {
                    const std::size_t pixel_index =
                        static_cast<std::size_t>(y) *
                        static_cast<std::size_t>(gbuffer.width()) +
                        static_cast<std::size_t>(x);

                    const GBuffer::Pixel& pixel =
                        gbuffer.pixel(x, y);

                    if (!pixel.valid) 
                    {
                        raw_[pixel_index] = {0.0f, 0.0f, 0.0f};
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

                    Math::Vec3 radiance = {0.0f, 0.0f, 0.0f};
                    float reflection_weight = 1.0f;

                    if (!reflectionUsesDetailedTrace(pixel.roughness)) 
                    {
                        const float sample_distance =
                            1.5f + pixel.roughness * 4.0f;

                        radiance = radiance_cache.sample(
                                Math::add(
                                        pixel.world_position,
                                        Math::multiply(
                                                reflection_direction,
                                                sample_distance
                                            )
                                    )
                            );

                        reflection_weight = Math::clamp(
                                1.0f - pixel.roughness * 0.65f,
                                0.15f,
                                1.0f
                            );
                    }
                    else 
                    {
                        const Math::Vec3 origin = Math::add(
                                pixel.world_position,
                                Math::multiply(pixel.normal, 0.035f)
                            );

                        const UnifiedTraceHit hit = tracer.trace(
                                gbuffer,
                                view,
                                projection,
                                origin,
                                reflection_direction,
                                40.0f
                            );

                        radiance = sampleReflectionRadiance(
                                hit.hit,
                                [&]()
                                {
                                    return surface_cache.radiance(
                                            hit.entity,
                                            hit.position,
                                            hit.normal
                                        );
                                },
                                [&]()
                                {
                                    return radiance_cache.sample(
                                            Math::add(
                                                    origin,
                                                    Math::multiply(
                                                            reflection_direction,
                                                            8.0f
                                                        )
                                                )
                                        );
                                }
                            );

                        const float smoothness = Math::clamp(
                                1.0f - pixel.roughness / 0.35f,
                                0.0f,
                                1.0f
                            );

                        reflection_weight =
                            0.65f + smoothness * 0.35f;
                    }

                    const Math::Vec3 fresnel = reflectionFresnel(
                            pixel,
                            view_direction
                        );

                    raw_[pixel_index] = Math::multiply(
                            Math::multiply(radiance, fresnel),
                            reflection_weight
                        );
                }
            }
        );

    if (width_ != gbuffer.width()
            || height_ != gbuffer.height()) 
    {
        width_ = gbuffer.width();
        height_ = gbuffer.height();
        history_.resize(width_, height_);
    }

    Temporal::resolveTaa(
            gbuffer,
            history_,
            raw_,
            output
        );

    history_.store(gbuffer, *output);
}

} // namespace Lumen
} // namespace Renderer
