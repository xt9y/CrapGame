#ifndef CRAPGAME_ECS_HPP
#define CRAPGAME_ECS_HPP

#include <cstdint>
#include <optional>
#include <vector>

namespace Ecs 
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

enum class MeshType 
{
    Cube,
    Plane,
};

struct MeshComponent 
{
    MeshType mesh;
};

struct RenderableComponent 
{
    bool visible;
};

struct MaterialComponent 
{
    Vec3 albedo,
         emissive;

    float metallic,
          roughness,
          emissive_strength;
};

struct LightComponent 
{
    Vec3 color;
};

class World 
{

public:
    Entity createEntity ();

    TransformComponent& addTransform (
                Entity entity,
                const TransformComponent& component
        );

    CameraComponent& addCamera (
                Entity entity,
                const CameraComponent& component
        );

    RenderableComponent& addRenderable (
                Entity entity,
                const RenderableComponent& component
        );

    MeshComponent& addMesh (
                Entity entity,
                const MeshComponent& component
        );

    MaterialComponent& addMaterial (
                Entity entity,
                const MaterialComponent& component
        );

    LightComponent& addLight (
                Entity entity,
                const LightComponent& component
        );

    TransformComponent *getTransform (Entity entity);
    const TransformComponent *getTransform (Entity entity) const;
    
    CameraComponent *getCamera (Entity entity);
    const CameraComponent *getCamera (Entity entity) const;
    
    RenderableComponent *getRenderable (Entity entity);
    const RenderableComponent *getRenderable (Entity entity) const;

    MeshComponent *getMesh (Entity entity);
    const MeshComponent *getMesh (Entity entity) const;

    MaterialComponent *getMaterial (Entity entity);
    const MaterialComponent *getMaterial (Entity entity) const;
    
    LightComponent *getLight (Entity entity);
    const LightComponent *getLight (Entity entity) const;

    Entity activeCamera () const;
    const std::vector<Entity>& entities () const;

private:
    void ensureCapacity (Entity entity);

    std::vector<Entity> entities_;
    std::vector<std::optional<TransformComponent>> transforms_;
    std::vector<std::optional<CameraComponent>> cameras_;
    std::vector<std::optional<RenderableComponent>> renderables_;
    std::vector<std::optional<MeshComponent>> meshes_;
    std::vector<std::optional<MaterialComponent>> materials_;
    std::vector<std::optional<LightComponent>> lights_;
};

} // namespace Ecs

#endif
