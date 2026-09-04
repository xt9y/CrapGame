#ifndef CRAPGAME_ECS_HPP
#define CRAPGAME_ECS_HPP

#include <cstdint>
#include <optional>
#include <vector>

namespace Ecs 
{

using Entity = std::uint32_t;
constexpr Entity INVALID_ENTITY = UINT32_MAX;
constexpr std::uint32_t INVALID_ASSET_HANDLE = UINT32_MAX;

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
    std::uint32_t loaded_mesh = INVALID_ASSET_HANDLE;
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

    Vec3 ambient = {0.0f, 0.0f, 0.0f},
         specular = {0.0f, 0.0f, 0.0f},
         transmission_color = {1.0f, 1.0f, 1.0f};

    float specular_strength = 1.0f,
          shininess = 0.0f,
          ior = 1.0f,
          opacity = 1.0f,
          transparency = 0.0f,
          transmission = 0.0f,
          reflectivity = 0.0f,
          clearcoat = 0.0f,
          clearcoat_roughness = 0.0f,
          sheen = 0.0f,
          anisotropy = 0.0f;

    int illumination_model = 0;
    std::uint32_t model_material = INVALID_ASSET_HANDLE;
    std::uint32_t renderer_material = INVALID_ASSET_HANDLE;
};

enum class LightType 
{
    Directional,
    Point,
    Spot,
};

struct LightComponent 
{
    LightType type;

    Vec3 color;

    float intensity,
          range,
          inner_cone,
          outer_cone,
          indirect_intensity;

    bool casts_shadows;
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

    std::uint64_t changeRevision () const { return change_revision_; }
    void markChanged () { ++change_revision_; }

private:
    void ensureCapacity (Entity entity);
    void touch () { ++change_revision_; }

    std::vector<Entity> entities_;
    std::vector<std::optional<TransformComponent>> transforms_;
    std::vector<std::optional<CameraComponent>> cameras_;
    std::vector<std::optional<RenderableComponent>> renderables_;
    std::vector<std::optional<MeshComponent>> meshes_;
    std::vector<std::optional<MaterialComponent>> materials_;
    std::vector<std::optional<LightComponent>> lights_;

    std::uint64_t change_revision_ = 1u;
    mutable std::uint64_t active_camera_revision_ = 0u;
    mutable Entity active_camera_cache_ = INVALID_ENTITY;
};

} // namespace Ecs

#endif
