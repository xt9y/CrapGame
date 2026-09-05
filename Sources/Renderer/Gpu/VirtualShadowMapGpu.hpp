#ifndef CRAPGAME_RENDERER_GPU_VIRTUALSHADOWMAPGPU_HPP
#define CRAPGAME_RENDERER_GPU_VIRTUALSHADOWMAPGPU_HPP

#include "Renderer/Gpu/ShadowPageCacheGpu.hpp"
#include "Renderer/Gpu/VirtualShadowPolicy.hpp"
#include "Renderer/Math/Math.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Ecs
{
class World;
}

namespace Renderer
{
namespace Gpu
{

class GBufferGpu;
class TriangleScene;

struct VirtualShadowClipmap
{
    Math::Vec3 origin = {0.0f, 0.0f, 0.0f};
    Math::Mat4 view_projection = {};
    float extent = 0.0f;
    float texel_world_size = 0.0f;
    int level = 0;
    int page_offset_x = 0;
    int page_offset_y = 0;
};

inline VirtualShadowClipmap directionalShadowClipmap (
            int level,
            const Math::Vec3& camera_position,
            const Math::Vec3& light_direction
    )
{
    const auto dot = [](
            const Math::Vec3& a,
            const Math::Vec3& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };

    const auto cross = [](
            const Math::Vec3& a,
            const Math::Vec3& b)
    {
        return Math::Vec3{
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x,
        };
    };

    const auto normalize = [](const Math::Vec3& value)
    {
        const float length = std::sqrt(
                value.x * value.x +
                value.y * value.y +
                value.z * value.z
            );

        if (length <= 0.000001f)
        {
            return Math::Vec3{0.0f, -1.0f, 0.0f};
        }

        return Math::Vec3{
            value.x / length,
            value.y / length,
            value.z / length,
        };
    };

    const Math::Vec3 forward = normalize(light_direction);
    const Math::Vec3 reference = std::fabs(forward.y) > 0.95f
        ? Math::Vec3{0.0f, 0.0f, 1.0f}
        : Math::Vec3{0.0f, 1.0f, 0.0f};
    const Math::Vec3 right = normalize(cross(reference, forward));
    const Math::Vec3 up = normalize(cross(forward, right));

    VirtualShadowClipmap result;
    result.level = level;
    result.extent = virtualShadowClipmapExtent(level);
    result.texel_world_size =
        result.extent * 2.0f /
        static_cast<float>(VirtualShadowPolicy::VIRTUAL_RESOLUTION);

    const float page_world_size =
        result.texel_world_size *
        static_cast<float>(VirtualShadowPolicy::PAGE_SIZE);

    const float light_x = dot(camera_position, right),
                light_y = dot(camera_position, up),
                light_z = dot(camera_position, forward);

    result.page_offset_x = static_cast<int>(
            std::floor(light_x / page_world_size)
        );
    result.page_offset_y = static_cast<int>(
            std::floor(light_y / page_world_size)
        );

    const float snapped_x =
        static_cast<float>(result.page_offset_x) * page_world_size;
    const float snapped_y =
        static_cast<float>(result.page_offset_y) * page_world_size;

    result.origin = {
        right.x * snapped_x + up.x * snapped_y + forward.x * light_z,
        right.y * snapped_x + up.y * snapped_y + forward.y * light_z,
        right.z * snapped_x + up.z * snapped_y + forward.z * light_z,
    };

    result.view_projection.value[0] = 1.0f;
    result.view_projection.value[5] = 1.0f;
    result.view_projection.value[10] = 1.0f;
    result.view_projection.value[15] = 1.0f;
    return result;
}

class VirtualShadowMapGpu
{
public:
    static constexpr int CLIPMAP_COUNT =
        VirtualShadowPolicy::LAST_CLIPMAP_LEVEL -
        VirtualShadowPolicy::FIRST_CLIPMAP_LEVEL + 1;

    bool init (std::string *error = nullptr);
    bool resize (int width, int height, std::string *error = nullptr);

    bool update (
                const Ecs::World& world,
                const GBufferGpu& gbuffer,
                const TriangleScene& triangles,
                const Math::Vec3& camera_position,
                std::uint64_t frame_index,
                std::uint64_t scene_revision,
                std::string *error = nullptr
        );

    bool bind (GLuint program, std::string *error = nullptr) const;
    void shutdown ();

    bool ready () const
    {
        return begin_program_ != 0
            && mark_program_ != 0
            && allocator_buffer_ != 0
            && clipmap_buffer_ != 0
            && page_cache_.metadataBuffer() != 0
            && page_cache_.pageTableBuffer() != 0
            && shadow_light_buffer_ != 0;
    }

    const ShadowPageCacheGpu& pageCache () const { return page_cache_; }
    ShadowPageCacheGpu& pageCache () { return page_cache_; }
    const std::array<VirtualShadowClipmap, CLIPMAP_COUNT>& clipmaps () const
    {
        return clipmaps_;
    }
    GLuint shadowLightBuffer () const { return shadow_light_buffer_; }
    GLuint allocatorBuffer () const { return allocator_buffer_; }
    GLuint clipmapBuffer () const { return clipmap_buffer_; }
    std::size_t shadowLightCount () const { return shadow_lights_.size(); }

private:
    struct ShadowLightGpu
    {
        float source_shape[4] = {};
    };

    struct ClipmapGpu
    {
        float view_projection[16] = {};
        float origin_extent[4] = {};
        std::int32_t page_offset_level[4] = {};
        float parameters[4] = {};
    };

    bool updateShadowLights (
                const Ecs::World& world,
                std::uint64_t scene_revision,
                std::string *error
        );
    void uploadClipmaps ();

    ShadowPageCacheGpu page_cache_;
    std::array<VirtualShadowClipmap, CLIPMAP_COUNT> clipmaps_ = {};
    std::array<ClipmapGpu, CLIPMAP_COUNT> clipmap_gpu_ = {};
    std::vector<ShadowLightGpu> shadow_lights_;

    GLuint begin_program_ = 0,
           mark_program_ = 0,
           allocator_buffer_ = 0,
           clipmap_buffer_ = 0,
           shadow_light_buffer_ = 0;

    GLint mark_inverse_view_projection_location_ = -1,
          mark_camera_location_ = -1,
          mark_frame_location_ = -1,
          mark_directional_light_location_ = -1,
          mark_clipmap_count_location_ = -1;

    std::size_t shadow_light_capacity_ = 0u;
    std::uint64_t shadow_light_revision_ = 0u;
    int width_ = 0,
        height_ = 0;
};

} // namespace Gpu
} // namespace Renderer

#endif
