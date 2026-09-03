#include "SphereTrace.hpp"

#include "Renderer/Mesh/Mesh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Renderer 
{
namespace Lumen 
{
namespace 
{

Math::Vec3 toVec3 (const Ecs::Vec3& value) 
{
    return {value.x, value.y, value.z};
}

float minimumScale (const Ecs::Vec3& scale) 
{
    return std::max(
            0.0001f,
            std::min(
                    std::fabs(scale.x),
                    std::min(
                            std::fabs(scale.y),
                            std::fabs(scale.z)
                        )
                )
        );
}

float maximumScale (const Ecs::Vec3& scale)
{
    return std::max(
            0.0001f,
            std::max(
                    std::fabs(scale.x),
                    std::max(
                            std::fabs(scale.y),
                            std::fabs(scale.z)
                        )
                )
        );
}

SdfWorldBounds transformBounds (
            const Mesh::Bounds& bounds,
            const Ecs::TransformComponent& transform
    )
{
    const Math::Mat4 model = Math::transform(
            toVec3(transform.position),
            toVec3(transform.rotation),
            toVec3(transform.scale)
        );

    SdfWorldBounds result = {};

    for (int corner_index = 0; corner_index < 8; ++corner_index)
    {
        const Math::Vec3 local = {
            (corner_index & 1) ? bounds.maximum.x : bounds.minimum.x,
            (corner_index & 2) ? bounds.maximum.y : bounds.minimum.y,
            (corner_index & 4) ? bounds.maximum.z : bounds.minimum.z,
        };

        const Math::Vec3 world = Math::transformPoint(model, local);

        if (corner_index == 0)
        {
            result.minimum = world;
            result.maximum = world;
            continue;
        }

        result.minimum.x = std::min(result.minimum.x, world.x);
        result.minimum.y = std::min(result.minimum.y, world.y);
        result.minimum.z = std::min(result.minimum.z, world.z);
        result.maximum.x = std::max(result.maximum.x, world.x);
        result.maximum.y = std::max(result.maximum.y, world.y);
        result.maximum.z = std::max(result.maximum.z, world.z);
    }

    return result;
}

float distanceToBounds (
                const Mesh::Bounds& bounds,
                const Math::Vec3& position
        ) 
{
    const float dx = std::max(
                std::max(
                        bounds.minimum.x - position.x,
                        0.0f
                    ),
                position.x - bounds.maximum.x
            ),
            dy = std::max(
                std::max(
                        bounds.minimum.y - position.y,
                        0.0f
                    ),
                position.y - bounds.maximum.y
            ),
            dz = std::max(
                std::max(
                        bounds.minimum.z - position.z,
                        0.0f
                    ),
                position.z - bounds.maximum.z
            );

    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool insideBounds (
                const Mesh::Bounds& bounds,
                const Math::Vec3& position
        ) 
{
    return position.x >= bounds.minimum.x
        && position.x <= bounds.maximum.x
        && position.y >= bounds.minimum.y
        && position.y <= bounds.maximum.y
        && position.z >= bounds.minimum.z
        && position.z <= bounds.maximum.z;
}

} // namespace

void DistanceFieldScene::build (const Ecs::World& world) 
{
    if (!fields_ready_) 
    {
        cube_ = buildDistanceField(
                Mesh::meshForType(Ecs::MeshType::Cube),
                28
            );

        plane_ = buildDistanceField(
                Mesh::meshForType(Ecs::MeshType::Plane),
                20
            );

        fields_ready_ = true;
    }

    instances_.clear();

    for (const Ecs::Entity entity : world.entities()) 
    {
        const Ecs::TransformComponent *transform =
            world.getTransform(entity);

        const Ecs::MeshComponent *mesh =
            world.getMesh(entity);

        const Ecs::RenderableComponent *renderable =
            world.getRenderable(entity);

        if (!transform
                || !mesh
                || !renderable
                || !renderable->visible) 
        {
            continue;
        }

        const MeshDistanceField& field = fieldFor(mesh->mesh);
        const float min_scale = minimumScale(transform->scale);
        const float max_scale = maximumScale(transform->scale);

        instances_.push_back({
            entity,
            mesh->mesh,
            *transform,
            cacheInverseTransform(*transform),
            transformBounds(field.bounds, *transform),
            min_scale / max_scale,
        });
    }
}

float DistanceFieldScene::distance (
                const Math::Vec3& position,
                Ecs::Entity *entity
        ) const 
{
    float nearest_distance =
        std::numeric_limits<float>::max();

    Ecs::Entity nearest_entity = Ecs::INVALID_ENTITY;

    for (const Instance& instance : instances_) 
    {
        /* World-AABB distance times min/max scale ratio is a conservative
         * lower bound for the existing local-distance*minimum-scale metric.
         * It therefore rejects only instances that cannot beat the current
         * nearest result, preserving exact renderer semantics. */
        const float lower_bound =
            sdfBoundsDistance(instance.world_bounds, position) *
            instance.broadphase_scale;

        if (lower_bound > nearest_distance)
        {
            continue;
        }

        const MeshDistanceField& field =
            fieldFor(instance.mesh);

        const Math::Vec3 local_position =
            inverseTransformPointCached(
                    position,
                    instance.inverse_transform
                );

        float local_distance = 0.0f;

        if (insideBounds(field.bounds, local_position)) 
        {
            local_distance = std::fabs(
                    sampleDistanceField(field, local_position)
                );
        }
        else 
        {
            local_distance =
                distanceToBounds(field.bounds, local_position);
        }

        const float world_distance =
            local_distance * minimumScale(instance.transform.scale);

        if (world_distance < nearest_distance) 
        {
            nearest_distance = world_distance;

            if (entity)
            {
                nearest_entity = instance.entity;
            }
        }
    }

    if (entity) 
    {
        *entity = nearest_entity;
    }

    return nearest_distance;
}

SdfHit DistanceFieldScene::trace (
                const Math::Vec3& origin,
                const Math::Vec3& direction,
                float maximum_distance,
                int maximum_steps,
                float minimum_step,
                float hit_epsilon
        ) const 
{
    SdfHit result;

    if (maximum_distance <= 0.0f
            || maximum_steps <= 0
            || instances_.empty()) 
    {
        return result;
    }

    const Math::Vec3 ray_direction =
        Math::normalize(direction);

    float travelled = std::max(hit_epsilon * 2.0f, minimum_step);

    for (int step = 0;
            step < maximum_steps && travelled <= maximum_distance;
            ++step) 
    {
        const Math::Vec3 position =
            Math::add(
                    origin,
                    Math::multiply(ray_direction, travelled)
                );

        Ecs::Entity entity = Ecs::INVALID_ENTITY;

        const float scene_distance =
            distance(position, &entity);

        if (scene_distance <= hit_epsilon) 
        {
            result.position = position;
            result.normal = normalAt(position);
            result.entity = entity;
            result.distance = travelled;
            result.hit = true;
            return result;
        }

        if (!std::isfinite(scene_distance)) 
        {
            break;
        }

        travelled += std::max(minimum_step, scene_distance * 0.80f);
    }

    return result;
}

SdfDistanceHit DistanceFieldScene::traceDistance (
                const Math::Vec3& origin,
                const Math::Vec3& direction,
                float maximum_distance,
                int maximum_steps,
                float minimum_step,
                float hit_epsilon
        ) const
{
    SdfDistanceHit result;

    if (maximum_distance <= 0.0f
            || maximum_steps <= 0
            || instances_.empty())
    {
        return result;
    }

    const Math::Vec3 ray_direction = Math::normalize(direction);
    float travelled = std::max(hit_epsilon * 2.0f, minimum_step);

    for (int step = 0;
            step < maximum_steps && travelled <= maximum_distance;
            ++step)
    {
        const Math::Vec3 position = Math::add(
                origin,
                Math::multiply(ray_direction, travelled)
            );

        const float scene_distance = distance(position, nullptr);

        if (scene_distance <= hit_epsilon)
        {
            result.distance = travelled;
            result.hit = true;
            return result;
        }

        if (!std::isfinite(scene_distance))
        {
            break;
        }

        travelled += std::max(minimum_step, scene_distance * 0.80f);
    }

    return result;
}

const MeshDistanceField& DistanceFieldScene::fieldFor (
                Ecs::MeshType mesh
        ) const 
{
    switch (mesh) 
    {
        case Ecs::MeshType::Cube:
            return cube_;

        case Ecs::MeshType::Plane:
            return plane_;
    }

    return cube_;
}

Math::Vec3 DistanceFieldScene::normalAt (
                const Math::Vec3& position
        ) const 
{
    constexpr float epsilon = 0.02f;

    const float dx = distance({position.x + epsilon, position.y, position.z}) -
                     distance({position.x - epsilon, position.y, position.z}),
                dy = distance({position.x, position.y + epsilon, position.z}) -
                     distance({position.x, position.y - epsilon, position.z}),
                dz = distance({position.x, position.y, position.z + epsilon}) -
                     distance({position.x, position.y, position.z - epsilon});

    return Math::normalize({dx, dy, dz});
}

} // namespace Lumen
} // namespace Renderer
