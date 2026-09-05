#ifndef CRAPGAME_RENDERER_GPU_SMRTSHADOWGPU_HPP
#define CRAPGAME_RENDERER_GPU_SMRTSHADOWGPU_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Math/Math.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cstdint>
#include <string>

namespace Renderer
{
namespace Gpu
{

class GBufferGpu;
class VirtualShadowMapGpu;

class SmrtShadowGpu
{
public:
    bool init (std::string *error = nullptr);
    bool resize (int width, int height, std::string *error = nullptr);

    bool render (
                const Ecs::World& world,
                const GBufferGpu& gbuffer,
                const VirtualShadowMapGpu& virtual_shadow_map,
                const Math::Vec3& camera_position,
                std::uint64_t frame_index,
                std::string *error = nullptr
        );

    void shutdown ();

    bool ready () const
    {
        return program_ != 0 && visibility_ != 0;
    }

    bool enabled () const { return light_index_ >= 0; }
    GLuint texture () const { return visibility_; }
    int lightIndex () const { return light_index_; }

private:
    GLuint program_ = 0,
           visibility_ = 0;

    GLint inverse_view_projection_location_ = -1,
          view_projection_location_ = -1,
          camera_location_ = -1,
          light_direction_location_ = -1,
          light_index_location_ = -1,
          light_type_location_ = -1,
          clipmap_count_location_ = -1,
          frame_index_location_ = -1;

    int width_ = 0,
        height_ = 0,
        light_index_ = -1;
};

} // namespace Gpu
} // namespace Renderer

#endif
