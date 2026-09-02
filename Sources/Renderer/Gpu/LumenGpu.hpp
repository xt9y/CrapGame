#ifndef CRAPGAME_RENDERER_GPU_LUMENGPU_HPP
#define CRAPGAME_RENDERER_GPU_LUMENGPU_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Gpu/DirectLightingGpu.hpp"
#include "Renderer/Gpu/GBufferGpu.hpp"
#include "Renderer/Math/Math.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Renderer
{
namespace Gpu
{

class LumenGpu
{
public:
    bool init (std::string *error = nullptr);
    bool resize (int width, int height, std::string *error = nullptr);

    bool render (
                const Ecs::World& world,
                const GBufferGpu& gbuffer,
                const DirectLightingGpu& direct,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Math::Vec3& camera_position,
                std::uint64_t frame_index,
                std::string *error = nullptr
        );

    void shutdown ();

    bool ready () const;
    GLuint finalTexture () const { return final_color_; }
    GLuint indirectTexture () const { return indirect_history_[history_index_]; }
    GLuint reflectionTexture () const { return reflection_history_[history_index_]; }

private:
    struct PrimitiveGpu
    {
        float position_type[4];
        float rotation[4];
        float scale[4];
        float albedo_metallic[4];
        float emissive_roughness[4];
    };

    bool uploadPrimitives (const Ecs::World& world, std::string *error);
    void destroyTextures ();

    GLuint trace_program_ = 0;
    GLuint composite_program_ = 0;
    GLuint primitive_buffer_ = 0;

    GLuint indirect_history_[2] = {0, 0};
    GLuint reflection_history_[2] = {0, 0};
    GLuint position_history_[2] = {0, 0};
    GLuint final_color_ = 0;

    GLint trace_view_projection_location_ = -1;
    GLint trace_camera_location_ = -1;
    GLint trace_primitive_count_location_ = -1;
    GLint trace_frame_location_ = -1;
    GLint trace_history_valid_location_ = -1;

    std::size_t primitive_capacity_ = 0;
    std::vector<PrimitiveGpu> primitives_;

    int history_index_ = 0;
    bool history_valid_ = false;

    int width_ = 0;
    int height_ = 0;
    int trace_width_ = 0;
    int trace_height_ = 0;
};

} // namespace Gpu
} // namespace Renderer

#endif
