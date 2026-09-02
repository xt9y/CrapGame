#include "SceneChanges.hpp"

#include <unordered_set>

namespace Renderer 
{
namespace Lumen 
{
namespace 
{

bool equalVec3 (
                const Ecs::Vec3& a,
                const Ecs::Vec3& b
        ) 
{
    return a.x == b.x
        && a.y == b.y
        && a.z == b.z;
}

bool equalTransform (
                const Ecs::TransformComponent& a,
                const Ecs::TransformComponent& b
        ) 
{
    return equalVec3(a.position, b.position)
        && equalVec3(a.rotation, b.rotation)
        && equalVec3(a.scale, b.scale);
}

bool equalCamera (
                const Ecs::CameraComponent& a,
                const Ecs::CameraComponent& b
        ) 
{
    return a.fov_degrees == b.fov_degrees
        && a.near_plane == b.near_plane
        && a.far_plane == b.far_plane
        && a.active == b.active;
}

bool equalRenderable (
                const Ecs::RenderableComponent& a,
                const Ecs::RenderableComponent& b
        ) 
{
    return a.visible == b.visible;
}

bool equalMesh (
                const Ecs::MeshComponent& a,
                const Ecs::MeshComponent& b
        ) 
{
    return a.mesh == b.mesh;
}

bool equalMaterial (
                const Ecs::MaterialComponent& a,
                const Ecs::MaterialComponent& b
        ) 
{
    return equalVec3(a.albedo, b.albedo)
        && equalVec3(a.emissive, b.emissive)
        && a.metallic == b.metallic
        && a.roughness == b.roughness
        && a.emissive_strength == b.emissive_strength;
}

bool equalLight (
                const Ecs::LightComponent& a,
                const Ecs::LightComponent& b
        ) 
{
    return a.type == b.type
        && equalVec3(a.color, b.color)
        && a.intensity == b.intensity
        && a.range == b.range
        && a.inner_cone == b.inner_cone
        && a.outer_cone == b.outer_cone
        && a.indirect_intensity == b.indirect_intensity
        && a.casts_shadows == b.casts_shadows;
}

} // namespace

ChangeTracker::Snapshot ChangeTracker::snapshot (
                const Ecs::World& world,
                Ecs::Entity entity
        ) const 
{
    Snapshot result;

    const Ecs::TransformComponent *transform =
        world.getTransform(entity);

    const Ecs::CameraComponent *camera =
        world.getCamera(entity);

    const Ecs::RenderableComponent *renderable =
        world.getRenderable(entity);

    const Ecs::MeshComponent *mesh =
        world.getMesh(entity);

    const Ecs::MaterialComponent *material =
        world.getMaterial(entity);

    const Ecs::LightComponent *light =
        world.getLight(entity);

    if (transform) 
    {
        result.transform = *transform;
        result.has_transform = true;
    }

    if (camera) 
    {
        result.camera = *camera;
        result.has_camera = true;
    }

    if (renderable) 
    {
        result.renderable = *renderable;
        result.has_renderable = true;
    }

    if (mesh) 
    {
        result.mesh = *mesh;
        result.has_mesh = true;
    }

    if (material) 
    {
        result.material = *material;
        result.has_material = true;
    }

    if (light) 
    {
        result.light = *light;
        result.has_light = true;
    }

    return result;
}

ChangeSet ChangeTracker::update (const Ecs::World& world) 
{
    ChangeSet changes;
    std::unordered_set<Ecs::Entity> current_entities;

    if (!initialized_) 
    {
        changes.geometry_changed = true;
        changes.material_changed = true;
        changes.lighting_changed = true;
        changes.camera_changed = true;
    }

    for (const Ecs::Entity entity : world.entities()) 
    {
        current_entities.insert(entity);

        const Snapshot current = snapshot(world, entity);
        const auto found = previous_.find(entity);

        if (found == previous_.end()) 
        {
            if (current.has_mesh
                    || current.has_renderable) 
            {
                changes.geometry_changed = true;
            }

            if (current.has_material) 
            {
                changes.material_changed = true;
            }

            if (current.has_light) 
            {
                changes.lighting_changed = true;
            }

            if (current.has_camera) 
            {
                changes.camera_changed = true;
            }

            previous_[entity] = current;
            continue;
        }

        const Snapshot& old = found->second;

        const bool transform_changed =
            current.has_transform != old.has_transform
            || (
                current.has_transform
                && !equalTransform(current.transform, old.transform)
            );

        const bool mesh_changed =
            current.has_mesh != old.has_mesh
            || (
                current.has_mesh
                && !equalMesh(current.mesh, old.mesh)
            );

        const bool renderable_changed =
            current.has_renderable != old.has_renderable
            || (
                current.has_renderable
                && !equalRenderable(current.renderable, old.renderable)
            );

        const bool material_changed =
            current.has_material != old.has_material
            || (
                current.has_material
                && !equalMaterial(current.material, old.material)
            );

        const bool light_changed =
            current.has_light != old.has_light
            || (
                current.has_light
                && !equalLight(current.light, old.light)
            );

        const bool camera_changed =
            current.has_camera != old.has_camera
            || (
                current.has_camera
                && !equalCamera(current.camera, old.camera)
            );

        if (mesh_changed
                || renderable_changed
                || (
                    transform_changed
                    && (current.has_mesh || old.has_mesh)
                )) 
        {
            changes.geometry_changed = true;
        }

        if (material_changed) 
        {
            changes.material_changed = true;
        }

        if (light_changed
                || (
                    transform_changed
                    && (current.has_light || old.has_light)
                )) 
        {
            changes.lighting_changed = true;
        }

        if (camera_changed
                || (
                    transform_changed
                    && (current.has_camera || old.has_camera)
                )) 
        {
            changes.camera_changed = true;
        }

        found->second = current;
    }

    for (auto iterator = previous_.begin();
            iterator != previous_.end();) 
    {
        if (current_entities.find(iterator->first) != current_entities.end()) 
        {
            ++iterator;
            continue;
        }

        if (iterator->second.has_mesh
                || iterator->second.has_renderable) 
        {
            changes.geometry_changed = true;
        }

        if (iterator->second.has_material) 
        {
            changes.material_changed = true;
        }

        if (iterator->second.has_light) 
        {
            changes.lighting_changed = true;
        }

        if (iterator->second.has_camera) 
        {
            changes.camera_changed = true;
        }

        iterator = previous_.erase(iterator);
    }

    initialized_ = true;
    return changes;
}

void ChangeTracker::clear () 
{
    previous_.clear();
    initialized_ = false;
}

} // namespace Lumen
} // namespace Renderer
