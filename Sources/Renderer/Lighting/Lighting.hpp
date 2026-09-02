#ifndef CRAPGAME_RENDERER_LIGHTING_HPP
#define CRAPGAME_RENDERER_LIGHTING_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/GBuffer/GBuffer.hpp"
#include "Renderer/Math/Math.hpp"

namespace Renderer 
{
namespace Lighting 
{

struct LightSample 
{
    Math::Vec3 direction,
               radiance;

    float distance;
    bool valid;
};

Math::Vec3 fresnelSchlick (
                float cosine,
                const Math::Vec3& f0
        );

float distributionGgx (
                const Math::Vec3& normal,
                const Math::Vec3& halfway,
                float roughness
        );

float geometrySmith (
                const Math::Vec3& normal,
                const Math::Vec3& view_direction,
                const Math::Vec3& light_direction,
                float roughness
        );

Math::Vec3 evaluatePbr (
                const Math::Vec3& albedo,
                float metallic,
                float roughness,
                const Math::Vec3& normal,
                const Math::Vec3& view_direction,
                const Math::Vec3& light_direction,
                const Math::Vec3& radiance
        );

LightSample sampleLight (
                const Ecs::LightComponent& light,
                const Ecs::TransformComponent& transform,
                const Math::Vec3& world_position
        );

Math::Vec3 evaluateDirect (
                const GBuffer::Pixel& pixel,
                const Math::Vec3& camera_position,
                const LightSample& light_sample,
                float visibility
        );

Math::Vec3 toneMap (const Math::Vec3& color);

} // namespace Lighting
} // namespace Renderer

#endif
