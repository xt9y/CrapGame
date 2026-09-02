#ifndef CRAPGAME_RENDERER_LUMEN_SCENECHANGES_HPP
#define CRAPGAME_RENDERER_LUMEN_SCENECHANGES_HPP

#include "Ecs/Ecs.hpp"

#include <unordered_map>

namespace Renderer 
{
namespace Lumen 
{

struct ChangeSet 
{
    bool geometry_changed = false,
         material_changed = false,
         lighting_changed = false,
         camera_changed = false;
};

class ChangeTracker 
{

public:
    ChangeSet update (const Ecs::World& world);
    void clear ();

private:
    struct Snapshot 
    {
        Ecs::TransformComponent transform = {};
        Ecs::CameraComponent camera = {};
        Ecs::RenderableComponent renderable = {};
        Ecs::MeshComponent mesh = {};
        Ecs::MaterialComponent material = {};
        Ecs::LightComponent light = {};

        bool has_transform = false,
             has_camera = false,
             has_renderable = false,
             has_mesh = false,
             has_material = false,
             has_light = false;
    };

    Snapshot snapshot (
                const Ecs::World& world,
                Ecs::Entity entity
        ) const;

    std::unordered_map<Ecs::Entity, Snapshot> previous_;
    bool initialized_ = false;
};

} // namespace Lumen
} // namespace Renderer

#endif
