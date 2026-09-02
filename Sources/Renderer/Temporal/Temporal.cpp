#include "Temporal.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer 
{
namespace Temporal 
{
namespace 
{

constexpr float EPSILON = 0.00001f;

Math::Vec3 toVec3 (const Ecs::Vec3& value) 
{
    return {value.x, value.y, value.z};
}

bool projectUv (
                const Math::Vec3& world_position,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                Math::Vec2 *uv
        ) 
{
    const Math::Vec4 view_position = 
        Math::transform(
                view,
                {
                    world_position.x,
                    world_position.y,
                    world_position.z,
                    1.0f
                }
            );

    const Math::Vec4 clip_position = 
        Math::transform(projection, view_position);

    if (clip_position.w <= EPSILON) 
    {
        return false;
    }

    const float inverse = 
        1.0f / clip_position.w;

    if (uv) 
    {
        uv->x = clip_position.x * inverse * 0.5f + 0.5f;
        uv->y = 1.0f - (clip_position.y * inverse * 0.5f + 0.5f);
    }

    return true;
}

} // namespace

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

void calculateMotion (
                GBuffer::Buffer *gbuffer,
                const Ecs::World& world,
                const FrameState& frame_state
        ) 
{
    if (!gbuffer) 
    {
        return;
    }

    for (int y = 0; y < gbuffer->height(); ++y) 
    {
        for (int x = 0; x < gbuffer->width(); ++x) 
        {
            GBuffer::Pixel& pixel = 
                gbuffer->pixel(x, y);

            pixel.motion = {0.0f, 0.0f};

            if (!pixel.valid 
                    || !frame_state.hasHistory()) 
            {
                continue;
            }

            const Ecs::TransformComponent *current_transform = 
                world.getTransform(pixel.entity);

            Ecs::TransformComponent previous_transform = {};

            if (!current_transform
                    || !frame_state.previousTransform(
                            pixel.entity,
                            &previous_transform
                        )) 
            {
                continue;
            }

            const Math::Vec3 local_position = 
                Math::inverseTransformPoint(
                        pixel.world_position,
                        toVec3(current_transform->position),
                        toVec3(current_transform->rotation),
                        toVec3(current_transform->scale)
                    );

            const Math::Mat4 previous_model = 
                Math::transform(
                        toVec3(previous_transform.position),
                        toVec3(previous_transform.rotation),
                        toVec3(previous_transform.scale)
                    );

            const Math::Vec3 previous_world_position = 
                Math::transformPoint(
                        previous_model,
                        local_position
                    );

            Math::Vec2 previous_uv = {};

            if (!projectUv(
                    previous_world_position,
                    frame_state.previousView(),
                    frame_state.previousProjection(),
                    &previous_uv
                )) 
            {
                continue;
            }

            const Math::Vec2 current_uv = {
                (static_cast<float>(x) + 0.5f) /
                    static_cast<float>(gbuffer->width()),
                (static_cast<float>(y) + 0.5f) /
                    static_cast<float>(gbuffer->height()),
            };

            pixel.motion = {
                current_uv.x - previous_uv.x,
                current_uv.y - previous_uv.y,
            };
        }
    }
}

void HistoryBuffer::resize (int width, int height) 
{
    width_  = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;

    pixels_.resize(
            static_cast<std::size_t>(width_) *
            static_cast<std::size_t>(height_)
        );

    clear();
}

void HistoryBuffer::clear () 
{
    const HistoryPixel empty = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        1.0f,
        Ecs::INVALID_ENTITY,
        false,
    };

    std::fill(pixels_.begin(), pixels_.end(), empty);
    has_history_ = false;
}

bool HistoryBuffer::sample (
                int x,
                int y,
                const GBuffer::Pixel& current,
                Math::Vec3 *color
        ) const 
{
    if (!has_history_ 
            || !current.valid) 
    {
        return false;
    }

    const float current_u =
        (static_cast<float>(x) + 0.5f) /
        static_cast<float>(width_);

    const float current_v =
        (static_cast<float>(y) + 0.5f) /
        static_cast<float>(height_);

    const float previous_u = current_u - current.motion.x,
                previous_v = current_v - current.motion.y;

    const int previous_x =
        static_cast<int>(
                std::floor(
                        previous_u * static_cast<float>(width_)
                    )
            );

    const int previous_y =
        static_cast<int>(
                std::floor(
                        previous_v * static_cast<float>(height_)
                    )
            );

    if (previous_x < 0 
            || previous_x >= width_
            || previous_y < 0 
            || previous_y >= height_) 
    {
        return false;
    }

    const HistoryPixel& previous =
        pixels_[index(previous_x, previous_y)];

    if (!previous.valid 
            || previous.entity != current.entity) 
    {
        return false;
    }

    if (std::fabs(previous.depth - current.depth) > 0.02f) 
    {
        return false;
    }

    const float normal_similarity =
        Math::dot(
                Math::normalize(previous.normal),
                Math::normalize(current.normal)
            );

    if (normal_similarity < 0.85f) 
    {
        return false;
    }

    if (color) 
    {
        *color = previous.color;
    }

    return true;
}

void HistoryBuffer::store (
                const GBuffer::Buffer& gbuffer,
                const std::vector<Math::Vec3>& color
        ) 
{
    if (gbuffer.width() != width_ 
            || gbuffer.height() != height_
            || color.size() != pixels_.size()) 
    {
        return;
    }

    for (int y = 0; y < height_; ++y) 
    {
        for (int x = 0; x < width_; ++x) 
        {
            const GBuffer::Pixel& source =
                gbuffer.pixel(x, y);

            HistoryPixel& destination =
                pixels_[index(x, y)];

            destination.color = color[index(x, y)];
            destination.normal = source.normal;
            destination.depth = source.depth;
            destination.entity = source.entity;
            destination.valid = source.valid;
        }
    }

    has_history_ = true;
}

bool HistoryBuffer::hasHistory () const 
{
    return has_history_;
}

std::size_t HistoryBuffer::index (int x, int y) const 
{
    return static_cast<std::size_t>(y) *
           static_cast<std::size_t>(width_) +
           static_cast<std::size_t>(x);
}

} // namespace Temporal
} // namespace Renderer
