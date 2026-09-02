#include "Lighting.hpp"

#include <cmath>
#include <limits>

namespace Renderer 
{
namespace Lighting 
{
namespace 
{

constexpr float PI = 3.14159265358979323846f;
constexpr float EPSILON = 0.00001f;

float geometrySchlickGgx (
                float cosine,
                float roughness
        ) 
{
    const float r = roughness + 1.0f,
                k = r * r / 8.0f;

    return cosine / 
        (cosine * (1.0f - k) + k + EPSILON);
}

Math::Vec3 toVec3 (const Ecs::Vec3& value) 
{
    return {value.x, value.y, value.z};
}

Math::Vec3 lightForward (const Ecs::TransformComponent& transform) 
{
    return Math::normalize(
            Math::transformDirection(
                    Math::rotationEuler(toVec3(transform.rotation)),
                    {0.0f, 0.0f, -1.0f}
                )
        );
}

float pointAttenuation (
                float distance,
                float range
        ) 
{
    if (range <= EPSILON 
            || distance >= range) 
    {
        return 0.0f;
    }

    const float ratio = distance / range,
                ratio_squared = ratio * ratio,
                range_factor = Math::saturate(
                        1.0f - ratio_squared * ratio_squared
                    );

    return range_factor * range_factor / 
        (distance * distance + 1.0f);
}

float spotAttenuation (
                const Ecs::LightComponent& light,
                const Ecs::TransformComponent& transform,
                const Math::Vec3& world_position
        ) 
{
    const Math::Vec3 light_position = 
        toVec3(transform.position);

    const Math::Vec3 from_light = 
        Math::normalize(
                Math::subtract(world_position, light_position)
            );

    const float cosine = 
        Math::dot(lightForward(transform), from_light);

    const float inner = 
        std::cos(Math::radians(light.inner_cone));

    const float outer = 
        std::cos(Math::radians(light.outer_cone));

    if (inner <= outer + EPSILON) 
    {
        return cosine >= outer ? 1.0f : 0.0f;
    }

    return Math::saturate(
            (cosine - outer) / (inner - outer)
        );
}

} // namespace

Math::Vec3 fresnelSchlick (
                float cosine,
                const Math::Vec3& f0
        ) 
{
    const float factor = 
        std::pow(1.0f - Math::saturate(cosine), 5.0f);

    return Math::add(
            f0,
            Math::multiply(
                    Math::subtract({1.0f, 1.0f, 1.0f}, f0),
                    factor
                )
        );
}

float distributionGgx (
                const Math::Vec3& normal,
                const Math::Vec3& halfway,
                float roughness
        ) 
{
    const float alpha = roughness * roughness,
                alpha_squared = alpha * alpha,
                cosine = Math::saturate(Math::dot(normal, halfway)),
                cosine_squared = cosine * cosine,
                denominator = cosine_squared * (alpha_squared - 1.0f) + 1.0f;

    return alpha_squared / 
        (PI * denominator * denominator + EPSILON);
}

float geometrySmith (
                const Math::Vec3& normal,
                const Math::Vec3& view_direction,
                const Math::Vec3& light_direction,
                float roughness
        ) 
{
    const float normal_view = 
        Math::saturate(Math::dot(normal, view_direction));

    const float normal_light = 
        Math::saturate(Math::dot(normal, light_direction));

    return geometrySchlickGgx(normal_view, roughness) *
           geometrySchlickGgx(normal_light, roughness);
}

Math::Vec3 evaluatePbr (
                const Math::Vec3& albedo,
                float metallic,
                float roughness,
                const Math::Vec3& normal,
                const Math::Vec3& view_direction,
                const Math::Vec3& light_direction,
                const Math::Vec3& radiance
        ) 
{
    const Math::Vec3 n = Math::normalize(normal),
                     v = Math::normalize(view_direction),
                     l = Math::normalize(light_direction),
                     h = Math::normalize(Math::add(v, l));

    const float normal_light = 
        Math::saturate(Math::dot(n, l));

    const float normal_view = 
        Math::saturate(Math::dot(n, v));

    if (normal_light <= 0.0f 
            || normal_view <= 0.0f) 
    {
        return {0.0f, 0.0f, 0.0f};
    }

    const Math::Vec3 dielectric = {0.04f, 0.04f, 0.04f};

    const Math::Vec3 f0 = 
        Math::mix(dielectric, albedo, Math::saturate(metallic));

    const Math::Vec3 fresnel = 
        fresnelSchlick(Math::dot(h, v), f0);

    const float distribution = 
        distributionGgx(n, h, Math::clamp(roughness, 0.04f, 1.0f));

    const float geometry = 
        geometrySmith(n, v, l, Math::clamp(roughness, 0.04f, 1.0f));

    const float denominator = 
        4.0f * normal_view * normal_light + EPSILON;

    const Math::Vec3 specular = 
        Math::multiply(fresnel, distribution * geometry / denominator);

    const Math::Vec3 diffuse_weight = 
        Math::multiply(
                Math::subtract({1.0f, 1.0f, 1.0f}, fresnel),
                1.0f - Math::saturate(metallic)
            );

    const Math::Vec3 diffuse = 
        Math::multiply(albedo, 1.0f / PI);

    return Math::multiply(
            Math::multiply(
                    Math::add(
                            Math::multiply(diffuse_weight, diffuse),
                            specular
                        ),
                    radiance
                ),
            normal_light
        );
}

LightSample sampleLight (
                const Ecs::LightComponent& light,
                const Ecs::TransformComponent& transform,
                const Math::Vec3& world_position
        ) 
{
    LightSample sample = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        0.0f,
        false,
    };

    const Math::Vec3 color = 
        toVec3(light.color);

    if (light.type == Ecs::LightType::Directional) 
    {
        sample.direction = 
            Math::multiply(lightForward(transform), -1.0f);

        sample.radiance = 
            Math::multiply(color, light.intensity);

        sample.distance = 
            std::numeric_limits<float>::max();

        sample.valid = light.intensity > 0.0f;
        return sample;
    }

    const Math::Vec3 light_position = 
        toVec3(transform.position);

    const Math::Vec3 to_light = 
        Math::subtract(light_position, world_position);

    const float distance = 
        Math::length(to_light);

    if (distance <= EPSILON) 
    {
        return sample;
    }

    const float attenuation = 
        pointAttenuation(distance, light.range);

    if (attenuation <= 0.0f) 
    {
        return sample;
    }

    float cone = 1.0f;

    if (light.type == Ecs::LightType::Spot) 
    {
        cone = spotAttenuation(
                light,
                transform,
                world_position
            );
    }

    if (cone <= 0.0f) 
    {
        return sample;
    }

    sample.direction = 
        Math::divide(to_light, distance);

    sample.radiance = 
        Math::multiply(
                color,
                light.intensity * attenuation * cone
            );

    sample.distance = distance;
    sample.valid = light.intensity > 0.0f;
    return sample;
}

Math::Vec3 evaluateDirect (
                const GBuffer::Pixel& pixel,
                const Math::Vec3& camera_position,
                const LightSample& light_sample,
                float visibility
        ) 
{
    if (!pixel.valid 
            || !light_sample.valid
            || visibility <= 0.0f) 
    {
        return {0.0f, 0.0f, 0.0f};
    }

    const Math::Vec3 view_direction = 
        Math::normalize(
                Math::subtract(
                        camera_position,
                        pixel.world_position
                    )
            );

    return evaluatePbr(
            pixel.albedo,
            pixel.metallic,
            pixel.roughness,
            pixel.normal,
            view_direction,
            light_sample.direction,
            Math::multiply(
                    light_sample.radiance,
                    Math::saturate(visibility)
                )
        );
}

Math::Vec3 toneMap (const Math::Vec3& color) 
{
    const Math::Vec3 mapped = {
        color.x / (1.0f + color.x),
        color.y / (1.0f + color.y),
        color.z / (1.0f + color.z),
    };

    constexpr float inverse_gamma = 1.0f / 2.2f;

    return {
        std::pow(Math::saturate(mapped.x), inverse_gamma),
        std::pow(Math::saturate(mapped.y), inverse_gamma),
        std::pow(Math::saturate(mapped.z), inverse_gamma),
    };
}

} // namespace Lighting
} // namespace Renderer
