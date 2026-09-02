#ifndef CRAPGAME_ECS_HPP
#define CRAPGAME_ECS_HPP

#include <cstdint>
#include <optional>
#include <vector>

namespace ecs 
{

using Entity = std::uint32_t;
constexpr Entity INVALID_ENTITY = UINT32_MAX;

struct Vec3 
{
    float x,
          y,
          z;
};

struct TransformComponent 
{
    Vec3 position,
         rotation,
         scale;
};

struct CameraComponent 
{
    float fov_degrees,
          near_plane,
          far_plane;
    
    bool active;
};

enum class Primitive 
{
    Cube,
    Plane,
};

struct RenderableComponent 
{
    Primitive primitive;
    
    Vec3 color;
};

LightComponent 
{
    Vec3 color;
}

class World 
{

public:
    Entity createEntity();

    TransformComponent& addTransform(Entity entity, const TransformComponent& component);
    CameraComponent& addCamera(Entity entity, const CameraComponent& component);
    RenderableComponent& addRenderable(Entity entity, const RenderableComponent& component);

    TransformComponent* getTransform(Entity entity);
    const TransformComponent* getTransform(Entity entity) const;
    
    CameraComponent* getCamera(Entity entity);
    const CameraComponent* getCamera(Entity entity) const;
    
    RenderableComponent* getRenderable(Entity entity);
    const RenderableComponent* getRenderable(Entity entity) const;
    
    LightComponent* getLight(Entity entity);
    const LightComponent getLight(Entity entity) const;

    Entity activeCamera() const;
    const std::vector<Entity>& entities() const;

private:
    void ensureCapacity(Entity entity);

    std::vector<Entity> entities_;
    std::vector<std::optional<TransformComponent>> transforms_;
    std::vector<std::optional<CameraComponent>> cameras_;
    std::vector<std::optional<RenderableComponent>> renderables_;
};

} // namespace ecs

#endif
