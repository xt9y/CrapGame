#include "Temporal.hpp"

namespace Renderer 
{
namespace Temporal 
{

void FrameState::capture (
                const Ecs::World& world,
                const Math::Mat4& view,
                const Math::Mat4& projection
        ) 
{
    previous_transforms_.clear();

    for (const Ecs::Entity entity : world.entities()) 
    {
        const Ecs::TransformComponent *transform = 
            world.getTransform(entity);

        if (!transform) 
        {
            continue;
        }

        previous_transforms_[entity] = *transform;
    }

    previous_view_ = view;
    previous_projection_ = projection;

    has_history_ = true;
    ++frame_index_;
}

bool FrameState::previousTransform (
                Ecs::Entity entity,
                Ecs::TransformComponent *transform
        ) const 
{
    const auto found = 
        previous_transforms_.find(entity);

    if (found == previous_transforms_.end()) 
    {
        return false;
    }

    if (transform) 
    {
        *transform = found->second;
    }

    return true;
}

const Math::Mat4& FrameState::previousView () const 
{
    return previous_view_;
}

const Math::Mat4& FrameState::previousProjection () const 
{
    return previous_projection_;
}

bool FrameState::hasHistory () const 
{
    return has_history_;
}

std::uint64_t FrameState::frameIndex () const 
{
    return frame_index_;
}

} // namespace Temporal
} // namespace Renderer
