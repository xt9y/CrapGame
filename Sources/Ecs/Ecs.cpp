#include "Ecs.hpp"

namespace Ecs 
{

void World::ensureCapacity (Entity entity) 
{
    const std::size_t size = 
        static_cast<std::size_t>(entity) + 1u;

    if (transforms_.size() < size) 
    {
        transforms_.resize(size);
    }

    if (cameras_.size() < size) 
    {
        cameras_.resize(size);
    }

    if (renderables_.size() < size) 
    {
        renderables_.resize(size);
    }

    if (meshes_.size() < size) 
    {
        meshes_.resize(size);
    }

    if (lights_.size() < size) 
    {
        lights_.resize(size);
    }
}

Entity World::createEntity () 
{
    const Entity entity = 
        static_cast<Entity>(entities_.size());

    entities_.push_back(entity);
    ensureCapacity(entity);
    return entity;
}

TransformComponent& World::addTransform (
                Entity entity,
                const TransformComponent& component
        ) 
{
    ensureCapacity(entity);
    transforms_[entity] = component;
    return *transforms_[entity];
}

CameraComponent& World::addCamera (
                Entity entity,
                const CameraComponent& component
        ) 
{
    ensureCapacity(entity);
    cameras_[entity] = component;
    return *cameras_[entity];
}

RenderableComponent& World::addRenderable (
                Entity entity,
                const RenderableComponent& component
        ) 
{
    ensureCapacity(entity);
    renderables_[entity] = component;
    return *renderables_[entity];
}

MeshComponent& World::addMesh (
                Entity entity,
                const MeshComponent& component
        ) 
{
    ensureCapacity(entity);
    meshes_[entity] = component;
    return *meshes_[entity];
}

LightComponent& World::addLight (
                Entity entity,
                const LightComponent& component
        ) 
{
    ensureCapacity(entity);
    lights_[entity] = component;
    return *lights_[entity];
}

TransformComponent *World::getTransform (Entity entity) 
{
    if (entity >= transforms_.size() 
            || !transforms_[entity]) 
    {
        return nullptr;
    }

    return &*transforms_[entity];
}

const TransformComponent *World::getTransform (Entity entity) const 
{
    if (entity >= transforms_.size() 
            || !transforms_[entity]) 
    {
        return nullptr;
    }

    return &*transforms_[entity];
}

CameraComponent *World::getCamera (Entity entity) 
{
    if (entity >= cameras_.size() 
            || !cameras_[entity]) 
    {
        return nullptr;
    }

    return &*cameras_[entity];
}

const CameraComponent *World::getCamera (Entity entity) const 
{
    if (entity >= cameras_.size() 
            || !cameras_[entity]) 
    {
        return nullptr;
    }

    return &*cameras_[entity];
}

RenderableComponent *World::getRenderable (Entity entity) 
{
    if (entity >= renderables_.size() 
            || !renderables_[entity]) 
    {
        return nullptr;
    }

    return &*renderables_[entity];
}

const RenderableComponent *World::getRenderable (Entity entity) const 
{
    if (entity >= renderables_.size() 
            || !renderables_[entity]) 
    {
        return nullptr;
    }

    return &*renderables_[entity];
}

MeshComponent *World::getMesh (Entity entity) 
{
    if (entity >= meshes_.size() 
            || !meshes_[entity]) 
    {
        return nullptr;
    }

    return &*meshes_[entity];
}

const MeshComponent *World::getMesh (Entity entity) const 
{
    if (entity >= meshes_.size() 
            || !meshes_[entity]) 
    {
        return nullptr;
    }

    return &*meshes_[entity];
}

LightComponent *World::getLight (Entity entity) 
{
    if (entity >= lights_.size() 
            || !lights_[entity]) 
    {
        return nullptr;
    }

    return &*lights_[entity];
}

const LightComponent *World::getLight (Entity entity) const 
{
    if (entity >= lights_.size() 
            || !lights_[entity]) 
    {
        return nullptr;
    }

    return &*lights_[entity];
}

Entity World::activeCamera () const 
{
    for (const Entity entity : entities_) 
    {
        const CameraComponent *camera = 
            getCamera(entity);

        if (camera 
                && camera->active 
                && getTransform(entity)) 
        {
            return entity;
        }
    }

    return INVALID_ENTITY;
}

const std::vector<Entity>& World::entities () const 
{
    return entities_;
}

} // namespace Ecs
