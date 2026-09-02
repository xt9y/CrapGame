#include "Lighting.hpp"

#include <cmath>

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

} // namespace Lighting
} // namespace Renderer
