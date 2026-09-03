#ifndef CRAPGAME_RENDERER_LUMEN_SPHERETRACE_HPP
#define CRAPGAME_RENDERER_LUMEN_SPHERETRACE_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Lumen/DistanceField.hpp"
#include "Renderer/Lumen/SdfBroadphase.hpp"
#include "Renderer/Lumen/SdfTransformCache.hpp"
#include "Renderer/Math/Math.hpp"

#include <vector>

namespace Renderer 
{
namespace Lumen 
{

struct SdfHit 
{
    Math::Vec3 position,
               normal;

    Ecs::Entity entity = Ecs::INVALID_ENTITY;

    float distance = 0.0f;
    bool hit = false;
};

struct SdfDistanceHit
{
    float distance = 0.0f;
    bool hit = false;
};

class DistanceFieldScene 
{

public:
    void build (const Ecs::World& world);

    float distance (
                const Math::Vec3& position,
                Ecs::Entity *entity = nullptr
        ) const;

    SdfHit trace (
                const Math::Vec3& origin,
                const Math::Vec3& direction,
                float maximum_distance,
                int maximum_steps = 96,
                float minimum_step = 0.01f,
                float hit_epsilon = 0.025f
        ) const;

    SdfDistanceHit traceDistance (
                const Math::Vec3& origin,
                const Math::Vec3& direction,
                float maximum_distance,
                int maximum_steps = 96,
                float minimum_step = 0.01f,
                float hit_epsilon = 0.025f
        ) const;

private:
    struct Instance 
    {
        Ecs::Entity entity;
        Ecs::MeshType mesh;
        Ecs::TransformComponent transform;
        CachedInverseTransform inverse_transform;
        SdfWorldBounds world_bounds;
        float broadphase_scale = 1.0f;
    };

    const MeshDistanceField& fieldFor (Ecs::MeshType mesh) const;
    Math::Vec3 normalAt (const Math::Vec3& position) const;

    MeshDistanceField cube_,
                      plane_;

    std::vector<Instance> instances_;
    bool fields_ready_ = false;
};

} // namespace Lumen
} // namespace Renderer

#endif
