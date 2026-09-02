#ifndef CRAPGAME_RENDERER_TEMPORAL_HPP
#define CRAPGAME_RENDERER_TEMPORAL_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/GBuffer/GBuffer.hpp"
#include "Renderer/Math/Math.hpp"

#include <cstdint>
#include <unordered_map>

namespace Renderer 
{
namespace Temporal 
{

class FrameState 
{

public:
    void capture (
                const Ecs::World& world,
                const Math::Mat4& view,
                const Math::Mat4& projection
        );

    bool previousTransform (
                Ecs::Entity entity,
                Ecs::TransformComponent *transform
        ) const;

    const Math::Mat4& previousView () const;
    const Math::Mat4& previousProjection () const;

    bool hasHistory () const;
    std::uint64_t frameIndex () const;

private:
    std::unordered_map<Ecs::Entity, Ecs::TransformComponent> previous_transforms_;

    Math::Mat4 previous_view_       = Math::identity(),
               previous_projection_ = Math::identity();

    std::uint64_t frame_index_ = 0;
    bool has_history_ = false;
};

void calculateMotion (
                GBuffer::Buffer *gbuffer,
                const Ecs::World& world,
                const FrameState& frame_state
        );

} // namespace Temporal
} // namespace Renderer

#endif
