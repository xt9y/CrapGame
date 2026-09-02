#include "Render.hpp"

#include "Renderer/Lighting/Lighting.hpp"
#include "Renderer/Lumen/ShortRangeAo.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace Renderer
{
namespace
{

bool matches (
                const std::string& test_name,
                std::initializer_list<const char *> names
        )
{
    for (const char *name : names)
    {
        if (test_name == name)
        {
            return true;
        }
    }

    return false;
}

Math::Vec3 normalColor (const Math::Vec3& normal)
{
    return {
        normal.x * 0.5f + 0.5f,
        normal.y * 0.5f + 0.5f,
        normal.z * 0.5f + 0.5f,
    };
}

Math::Vec3 distanceColor (float distance)
{
    const float magnitude =
        Math::saturate(std::fabs(distance) * 3.0f);

    if (distance < 0.0f)
    {
        return {
            1.0f,
            magnitude * 0.15f,
            magnitude * 0.15f,
        };
    }

    return {
        magnitude * 0.15f,
        0.35f + magnitude * 0.35f,
        1.0f - magnitude * 0.40f,
    };
}

} // namespace

bool Rendering::setTestName (const char *test_name)
{
    test_name_ = test_name
        ? test_name
        : "";

    if (test_name_.empty())
    {
        render_mode_ = RenderMode::Final;
        return true;
    }

    if (matches(test_name_, {
            "MaterialAlbedo",
            "GBufferAlbedo",
        }))
    {
        render_mode_ = RenderMode::Albedo;
        return true;
    }

    if (matches(test_name_, {
            "Normals",
            "GBufferNormal",
        }))
    {
        render_mode_ = RenderMode::Normal;
        return true;
    }

    if (matches(test_name_, {
            "Depth",
            "GBufferDepth",
        }))
    {
        render_mode_ = RenderMode::Depth;
        return true;
    }

    if (matches(test_name_, {
            "MaterialRoughness",
            "MaterialMetallic",
            "MaterialEmissive",
            "GBufferMaterial",
        }))
    {
        render_mode_ = RenderMode::Material;
        return true;
    }

    if (matches(test_name_, {
            "DirectPoint",
            "DirectDirectional",
            "DirectSpot",
            "LightFalloff",
            "LightColor",
            "PbrDiffuse",
            "PbrSpecular",
            "PbrFresnel",
            "LumenDirectOnly",
        }))
    {
        render_mode_ = RenderMode::Direct;
        return true;
    }

    if (matches(test_name_, {
            "ShadowPoint",
            "ShadowDirectional",
            "ShadowSpot",
            "ShadowBias",
        }))
    {
        render_mode_ = RenderMode::Shadow;
        return true;
    }

    if (matches(test_name_, {
            "MotionVectors",
        }))
    {
        render_mode_ = RenderMode::Motion;
        return true;
    }

    if (matches(test_name_, {
            "MeshSdfCube",
            "MeshSdfPlane",
            "SdfInsideOutside",
        }))
    {
        render_mode_ = RenderMode::MeshSdf;
        return true;
    }

    if (matches(test_name_, {
            "GlobalSdf",
        }))
    {
        render_mode_ = RenderMode::GlobalSdf;
        return true;
    }

    if (matches(test_name_, {
            "ScreenTraceHit",
            "ScreenTraceMiss",
            "SdfTraceHit",
            "SdfTraceMiss",
            "LumenTraceFallback",
        }))
    {
        render_mode_ = RenderMode::Trace;
        return true;
    }

    if (matches(test_name_, {
            "LumenCardCoverage",
            "SurfaceCacheAlbedo",
            "SurfaceCacheNormal",
        }))
    {
        render_mode_ = RenderMode::SurfaceCache;
        return true;
    }

    if (matches(test_name_, {
            "SurfaceCacheLighting",
            "SurfaceCacheDirty",
        }))
    {
        render_mode_ = RenderMode::SurfaceLighting;
        return true;
    }

    if (matches(test_name_, {
            "RadianceProbe",
            "RadianceCache",
            "RadianceCacheDirty",
        }))
    {
        render_mode_ = RenderMode::RadianceCache;
        return true;
    }

    if (matches(test_name_, {
            "ScreenProbePlacement",
            "ScreenProbeTrace",
            "ScreenProbeGather",
        }))
    {
        render_mode_ = RenderMode::ScreenProbes;
        return true;
    }

    if (matches(test_name_, {
            "LumenIndirectOnly",
            "LumenBounceOne",
            "LumenMultiBounce",
            "LumenColorBleed",
            "LumenOcclusion",
            "EmissiveGi",
            "MovingLightGi",
            "MovingObjectGi",
            "LumenConvergence",
        }))
    {
        render_mode_ = RenderMode::Indirect;
        return true;
    }

    if (matches(test_name_, {
            "ShortRangeAo",
        }))
    {
        render_mode_ = RenderMode::Ao;
        return true;
    }

    if (matches(test_name_, {
            "ReflectionRough",
            "ReflectionSmooth",
            "ReflectionOffscreen",
            "ReflectionTemporal",
        }))
    {
        render_mode_ = RenderMode::Reflection;
        return true;
    }

    if (matches(test_name_, {
            "GeometryCube",
            "GeometryPlane",
            "CameraPerspective",
            "TemporalStatic",
            "TemporalMotion",
            "TaaEdges",
            "LumenFinal",
            "LumenStress",
            "FinalScene",
        }))
    {
        render_mode_ = RenderMode::Final;
        return true;
    }

    return false;
}

int Rendering::captureFrame (std::uint64_t frame)
{
    if (!rendercheck_capture_due(frame))
    {
        return 0;
    }

    if (width_ <= 0
            || height_ <= 0
            || color_buffer_.size()
                != static_cast<std::size_t>(width_) *
                   static_cast<std::size_t>(height_) * 3u)
    {
        return -1;
    }

    const std::size_t row_bytes =
        static_cast<std::size_t>(width_) * 3u;

    const int result = rendercheck_capture_rgb8(
            color_buffer_.data(),
            static_cast<std::uint32_t>(width_),
            static_cast<std::uint32_t>(height_),
            row_bytes
        );

    return result < 0
        ? -1
        : 0;
}

void Rendering::composeDebug (
                const Ecs::World& world,
                const Math::Vec3& camera_position
        )
{
    const Math::Vec3 clear_color = {
        0.055f, 0.070f, 0.105f
    };

    const std::size_t pixel_count =
        static_cast<std::size_t>(width_) *
        static_cast<std::size_t>(height_);

    frame_color_.assign(pixel_count, clear_color);

    for (int y = 0; y < height_; ++y)
    {
        for (int x = 0; x < width_; ++x)
        {
            const std::size_t index =
                static_cast<std::size_t>(y) *
                static_cast<std::size_t>(width_) +
                static_cast<std::size_t>(x);

            const GBuffer::Pixel& pixel =
                gbuffer_.pixel(x, y);

            if (!pixel.valid)
            {
                continue;
            }

            Math::Vec3 color = clear_color;

            if (render_mode_ == RenderMode::Albedo)
            {
                color = pixel.albedo;
            }

            if (render_mode_ == RenderMode::Normal)
            {
                color = normalColor(pixel.normal);
            }

            if (render_mode_ == RenderMode::Depth)
            {
                const float depth =
                    Math::saturate((1.0f - pixel.depth) * 28.0f);

                color = {depth, depth, depth};
            }

            if (render_mode_ == RenderMode::Material)
            {
                const float emissive =
                    Math::saturate(Math::length(pixel.emissive));

                color = {
                    Math::saturate(pixel.metallic),
                    Math::saturate(pixel.roughness),
                    emissive,
                };
            }

            if (render_mode_ == RenderMode::Direct)
            {
                color = Lighting::toneMap(
                        direct_color_[index]
                    );
            }

            if (render_mode_ == RenderMode::Shadow)
            {
                float visibility = 1.0f;
                bool sampled = false;

                for (const Ecs::Entity entity : world.entities())
                {
                    const Ecs::TransformComponent *transform =
                        world.getTransform(entity);

                    const Ecs::LightComponent *light =
                        world.getLight(entity);

                    if (!transform
                            || !light
                            || !light->casts_shadows)
                    {
                        continue;
                    }

                    const Lighting::LightSample light_sample =
                        Lighting::sampleLight(
                                *light,
                                *transform,
                                pixel.world_position
                            );

                    if (!light_sample.valid)
                    {
                        continue;
                    }

                    visibility = std::min(
                            visibility,
                            shadows_.visibility(
                                    pixel.world_position,
                                    pixel.normal,
                                    light_sample
                                )
                        );

                    sampled = true;
                }

                if (!sampled)
                {
                    visibility = 1.0f;
                }

                color = {
                    visibility,
                    visibility,
                    visibility,
                };
            }

            if (render_mode_ == RenderMode::Motion)
            {
                color = {
                    Math::saturate(0.5f + pixel.motion.x * 6.0f),
                    Math::saturate(0.5f + pixel.motion.y * 6.0f),
                    0.5f,
                };
            }

            if (render_mode_ == RenderMode::MeshSdf)
            {
                const Math::Vec3 sample_position = Math::add(
                        pixel.world_position,
                        Math::multiply(pixel.normal, 0.18f)
                    );

                const float distance =
                    tracer_.distanceFieldScene().distance(
                            sample_position
                        );

                color = distanceColor(distance);
            }

            if (render_mode_ == RenderMode::GlobalSdf)
            {
                const Math::Vec3 sample_position = Math::add(
                        pixel.world_position,
                        Math::multiply(pixel.normal, 0.18f)
                    );

                const float distance =
                    tracer_.globalDistanceField().sample(
                            sample_position
                        );

                color = distanceColor(distance);
            }

            if (render_mode_ == RenderMode::Trace)
            {
                const Math::Vec3 view_direction = Math::normalize(
                        Math::subtract(
                                camera_position,
                                pixel.world_position
                            )
                    );

                const Math::Vec3 direction = Math::normalize(
                        Math::reflect(
                                Math::multiply(
                                        view_direction,
                                        -1.0f
                                    ),
                                pixel.normal
                            )
                    );

                const Lumen::UnifiedTraceHit hit = tracer_.trace(
                        gbuffer_,
                        view_,
                        projection_,
                        Math::add(
                                pixel.world_position,
                                Math::multiply(
                                        pixel.normal,
                                        0.04f
                                    )
                            ),
                        direction,
                        20.0f
                    );

                if (!hit.hit)
                {
                    color = {1.0f, 0.08f, 0.05f};
                }

                if (hit.hit
                        && hit.source == Lumen::TraceSource::Screen)
                {
                    color = {0.05f, 1.0f, 0.15f};
                }

                if (hit.hit
                        && hit.source == Lumen::TraceSource::DistanceField)
                {
                    color = {0.08f, 0.25f, 1.0f};
                }
            }

            if (render_mode_ == RenderMode::SurfaceCache)
            {
                const Lumen::SurfaceSample *sample =
                    surface_cache_.sample(
                            pixel.entity,
                            pixel.world_position,
                            pixel.normal
                        );

                if (!sample)
                {
                    color = {1.0f, 0.0f, 0.0f};
                }
                else if (test_name_ == "SurfaceCacheNormal")
                {
                    color = normalColor(sample->card.normal);
                }
                else if (test_name_ == "LumenCardCoverage")
                {
                    color = {0.05f, 0.90f, 0.18f};
                }
                else
                {
                    color = sample->albedo;
                }
            }

            if (render_mode_ == RenderMode::SurfaceLighting)
            {
                color = Lighting::toneMap(
                        surface_cache_.radiance(
                                pixel.entity,
                                pixel.world_position,
                                pixel.normal
                            )
                    );
            }

            if (render_mode_ == RenderMode::RadianceCache)
            {
                color = Lighting::toneMap(
                        radiance_cache_.sample(
                                pixel.world_position
                            )
                    );
            }

            if (render_mode_ == RenderMode::ScreenProbes)
            {
                color = Lighting::toneMap(
                        Math::multiply(
                                indirect_color_[index],
                                3.0f
                            )
                    );
            }

            if (render_mode_ == RenderMode::Indirect)
            {
                color = Lighting::toneMap(
                        Math::multiply(
                                indirect_resolved_[index],
                                3.0f
                            )
                    );
            }

            if (render_mode_ == RenderMode::Ao)
            {
                const float visibility =
                    Lumen::shortRangeVisibility(
                            gbuffer_,
                            view_,
                            projection_,
                            tracer_,
                            pixel,
                            frame_state_.frameIndex(),
                            2,
                            0.80f
                        );

                color = {
                    visibility,
                    visibility,
                    visibility,
                };
            }

            if (render_mode_ == RenderMode::Reflection)
            {
                color = Lighting::toneMap(
                        Math::multiply(
                                reflection_color_[index],
                                2.5f
                            )
                    );
            }

            frame_color_[index] = color;
        }
    }
}

} // namespace Renderer
