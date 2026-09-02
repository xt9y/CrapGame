#ifndef CRAPGAME_RENDERER_SHADOWS_HPP
#define CRAPGAME_RENDERER_SHADOWS_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Lighting/Lighting.hpp"
#include "Renderer/Math/Math.hpp"

#include <vector>

namespace Renderer 
{
namespace Shadows 
{

struct Triangle 
{
    Math::Vec3 a,
               b,
               c;

    Ecs::Entity entity;
};

bool intersectTriangle (
                const Math::Vec3& origin,
                const Math::Vec3& direction,
                const Triangle& triangle,
                float maximum_distance,
                float *distance
        );

class Scene 
{

public:
    void build (const Ecs::World& world);

    float visibility (
                const Math::Vec3& world_position,
                const Math::Vec3& normal,
                const Lighting::LightSample& light_sample
        ) const;

    std::size_t triangleCount () const;

private:
    std::vector<Triangle> triangles_;
};

} // namespace Shadows
} // namespace Renderer

#endif
