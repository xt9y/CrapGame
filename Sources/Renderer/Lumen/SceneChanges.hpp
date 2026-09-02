#ifndef CRAPGAME_RENDERER_LUMEN_SCENECHANGES_HPP
#define CRAPGAME_RENDERER_LUMEN_SCENECHANGES_HPP

#include "Ecs/Ecs.hpp"

#include <cstdint>
#include <vector>

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

    /* World entity IDs are contiguous and monotonic; World has no removal
     * API. A vector therefore avoids per-frame unordered_map/set hashing and
     * allocation in this render-hot change detector. */
    std::vector<Snapshot> previous_;
    std::uint64_t previous_world_revision_ = 0;
    bool initialized_ = false;
};

} // namespace Lumen
} // namespace Renderer

#endif
