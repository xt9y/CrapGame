#include "SceneLighting.hpp"

#include "Renderer/Lighting/Lighting.hpp"

#include <vector>

namespace Renderer 
{
namespace Lumen 
{
namespace 
{

constexpr float PI = 3.14159265358979323846f;

struct ActiveLight 
{
    const Ecs::TransformComponent *transform;
    const Ecs::LightComponent *light;
};

} // namespace

void updateSceneLighting (
                SurfaceCache *surface_cache,
                const Ecs::World& world,
                const Shadows::Scene& shadows
        ) 
{
    if (!surface_cache) 
    {
        return;
    }

    std::vector<ActiveLight> lights;

    for (const Ecs::Entity entity : world.entities()) 
    {
        const Ecs::TransformComponent *transform =
            world.getTransform(entity);

        const Ecs::LightComponent *light =
            world.getLight(entity);

        if (transform && light) 
        {
            lights.push_back({transform, light});
        }
    }

    for (SurfaceSample& surface : surface_cache->samples()) 
    {
        Math::Vec3 irradiance = {0.0f, 0.0f, 0.0f};

        for (const ActiveLight& active_light : lights) 
        {
            const Lighting::LightSample light_sample =
                Lighting::sampleLight(
                        *active_light.light,
                        *active_light.transform,
                        surface.card.position
                    );

            if (!light_sample.valid) 
            {
                continue;
            }

            const float cosine = Math::dot(
                    surface.card.normal,
                    light_sample.direction
                );

            if (cosine <= 0.0f) 
            {
                continue;
            }

            const float visibility = shadows.visibility(
                    surface.card.position,
                    surface.card.normal,
                    light_sample
                );

            irradiance = Math::add(
                    irradiance,
                    Math::multiply(
                            light_sample.radiance,
                            cosine * visibility
                        )
                );
        }

        surface.direct_lighting = Math::multiply(
                Math::Vec3{
                    surface.albedo.x * irradiance.x,
                    surface.albedo.y * irradiance.y,
                    surface.albedo.z * irradiance.z,
                },
                1.0f / PI
            );
    }
}

} // namespace Lumen
} // namespace Renderer
