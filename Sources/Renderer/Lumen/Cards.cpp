#include "Cards.hpp"

#include "Renderer/Mesh/Mesh.hpp"

#include <algorithm>
#include <cmath>

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

void addCard (
                std::vector<Card> *cards,
                Ecs::Entity entity,
                const Math::Mat4& model,
                const Math::Vec3& local_position,
                const Math::Vec3& local_normal,
                const Math::Vec3& local_u,
                const Math::Vec3& local_v,
                float extent_u,
                float extent_v
        ) 
{
    if (!cards
            || extent_u <= 0.0001f
            || extent_v <= 0.0001f) 
    {
        return;
    }

    cards->push_back({
        entity,
        Math::transformPoint(model, local_position),
        Math::normalize(Math::transformDirection(model, local_normal)),
        Math::normalize(Math::transformDirection(model, local_u)),
        Math::normalize(Math::transformDirection(model, local_v)),
        extent_u,
        extent_v,
    });
}

} // namespace

void CardScene::build (const Ecs::World& world) 
{
    cards_.clear();

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

        const Mesh::Bounds bounds =
            Mesh::meshForType(mesh->mesh).bounds;

        const Math::Vec3 center = {
            (bounds.minimum.x + bounds.maximum.x) * 0.5f,
            (bounds.minimum.y + bounds.maximum.y) * 0.5f,
            (bounds.minimum.z + bounds.maximum.z) * 0.5f,
        };

        const Math::Vec3 extent = {
            (bounds.maximum.x - bounds.minimum.x) * 0.5f,
            (bounds.maximum.y - bounds.minimum.y) * 0.5f,
            (bounds.maximum.z - bounds.minimum.z) * 0.5f,
        };

        const Math::Mat4 model =
            Math::transform(
                    toVec3(transform->position),
                    toVec3(transform->rotation),
                    toVec3(transform->scale)
                );

        const float scale_x = std::fabs(transform->scale.x),
                    scale_y = std::fabs(transform->scale.y),
                    scale_z = std::fabs(transform->scale.z);

        addCard(&cards_, entity, model,
                {bounds.maximum.x, center.y, center.z}, {1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
                extent.y * scale_y, extent.z * scale_z);

        addCard(&cards_, entity, model,
                {bounds.minimum.x, center.y, center.z}, {-1.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f},
                extent.y * scale_y, extent.z * scale_z);

        addCard(&cards_, entity, model,
                {center.x, bounds.maximum.y, center.z}, {0.0f, 1.0f, 0.0f},
                {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f},
                extent.x * scale_x, extent.z * scale_z);

        addCard(&cards_, entity, model,
                {center.x, bounds.minimum.y, center.z}, {0.0f, -1.0f, 0.0f},
                {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
                extent.x * scale_x, extent.z * scale_z);

        addCard(&cards_, entity, model,
                {center.x, center.y, bounds.maximum.z}, {0.0f, 0.0f, 1.0f},
                {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
                extent.x * scale_x, extent.y * scale_y);

        addCard(&cards_, entity, model,
                {center.x, center.y, bounds.minimum.z}, {0.0f, 0.0f, -1.0f},
                {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f},
                extent.x * scale_x, extent.y * scale_y);
    }
}

const std::vector<Card>& CardScene::cards () const 
{
    return cards_;
}

} // namespace Lumen
} // namespace Renderer
