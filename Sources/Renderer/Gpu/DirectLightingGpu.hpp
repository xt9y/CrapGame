#ifndef CRAPGAME_RENDERER_GPU_DIRECTLIGHTINGGPU_HPP
#define CRAPGAME_RENDERER_GPU_DIRECTLIGHTINGGPU_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Gpu/GBufferGpu.hpp"
#include "Renderer/Math/Math.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cstddef>
#include <string>
#include <vector>

namespace Renderer
{
namespace Gpu
{

class DirectLightingGpu
{
public:
    bool init (std::string *error = nullptr);
    bool resize (int width, int height, std::string *error = nullptr);

    bool updateScene (
                const Ecs::World& world,
                std::string *error = nullptr
        );

    bool dispatch (
                const GBufferGpu& gbuffer,
                const Math::Vec3& camera_position,
                std::string *error = nullptr
        );

    bool render (
                const Ecs::World& world,
                const GBufferGpu& gbuffer,
                const Math::Vec3& camera_position,
                std::string *error = nullptr
        );

    void shutdown ();

    bool ready () const { return program_ != 0 && direct_color_ != 0 && final_color_ != 0; }
    GLuint directTexture () const { return direct_color_; }
    GLuint finalTexture () const { return final_color_; }

    /* The analytic-shadow primitive layout is intentionally shared with
     * LumenGpu. One ECS extraction/upload feeds both compute pipelines. */
    GLuint primitiveBuffer () const { return primitive_buffer_; }
    std::size_t primitiveCount () const { return primitives_.size(); }

private:
    struct LightGpu
    {
        float position_type[4];
        float direction_range[4];
        float color_intensity[4];
        float cone_shadow[4];
    };

    struct PrimitiveGpu
    {
        float position_type[4];
        float rotation[4];
        float scale[4];
        float albedo_metallic[4];
        float emissive_roughness[4];
    };

    bool uploadBuffer (
                GLuint buffer,
                std::size_t *capacity,
                const void *data,
                std::size_t size,
                std::string *error
        );

    void destroyTextures ();

    GLuint program_ = 0;
    GLuint light_buffer_ = 0;
    GLuint primitive_buffer_ = 0;
    GLuint direct_color_ = 0;
    GLuint final_color_ = 0;

    GLint camera_location_ = -1;
    GLint light_count_location_ = -1;
    GLint primitive_count_location_ = -1;

    std::size_t light_capacity_ = 0;
    std::size_t primitive_capacity_ = 0;

    std::vector<LightGpu> lights_;
    std::vector<PrimitiveGpu> primitives_;
    std::vector<LightGpu> uploaded_lights_;
    std::vector<PrimitiveGpu> uploaded_primitives_;

    int width_ = 0;
    int height_ = 0;
};

} // namespace Gpu
} // namespace Renderer

#endif
