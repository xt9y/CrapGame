#ifndef CRAPGAME_RENDERER_LIGHTING_HPP
#define CRAPGAME_RENDERER_LIGHTING_HPP

#include "Renderer/Math/Math.hpp"

namespace Renderer 
{
namespace Lighting 
{

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

} // namespace Lighting
} // namespace Renderer

#endif
