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

    /* Legacy reference entrypoint. Kept while the GPU migration remains easy
     * to bisect; interactive rendering uses traceShared/composite below. */
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

    /* Trace against DirectLightingGpu's already-populated primitive SSBO.
     * This removes Lumen's duplicate ECS walk and glBufferSubData upload. */
    bool traceShared (
                const GBufferGpu& gbuffer,
                const DirectLightingGpu& direct,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Math::Vec3& camera_position,
                std::uint64_t frame_index,
                std::string *error = nullptr
        )
    {
        if (!ready()
                || !gbuffer.ready()
                || !direct.ready()
                || direct.primitiveBuffer() == 0
                || gbuffer.width() != width_
                || gbuffer.height() != height_)
        {
            setInlineError(error, "GPU Lumen shared trace resources are not ready");
            return false;
        }

        const Math::Mat4 view_projection = Math::multiply(projection, view);
        const int read_index = history_index_;
        const int write_index = 1 - history_index_;

        bindTextureUnitInline(0, gbuffer.positionDepthTexture());
        bindTextureUnitInline(1, gbuffer.normalRoughnessTexture());
        bindTextureUnitInline(2, gbuffer.albedoMetallicTexture());
        bindTextureUnitInline(3, direct.directTexture());
        bindTextureUnitInline(4, indirect_history_[read_index]);
        bindTextureUnitInline(5, reflection_history_[read_index]);
        bindTextureUnitInline(6, position_history_[read_index]);

        GL20.glUseProgram(trace_program_);
        GL20.glUniformMatrix4fv(
                trace_view_projection_location_,
                1,
                GL_FALSE,
                view_projection.value
            );
        GL20.glUniform3f(
                trace_camera_location_,
                camera_position.x,
                camera_position.y,
                camera_position.z
            );
        GL20.glUniform1i(
                trace_primitive_count_location_,
                static_cast<GLint>(direct.primitiveCount())
            );
        GL20.glUniform1i(
                trace_frame_location_,
                static_cast<GLint>(frame_index & 0x7fffffffu)
            );
        GL20.glUniform1i(
                trace_history_valid_location_,
                history_valid_ ? 1 : 0
            );

        GL30.glBindBufferBase(
                GL_SHADER_STORAGE_BUFFER,
                7,
                direct.primitiveBuffer()
            );
        GL42.glBindImageTexture(
                0,
                indirect_history_[write_index],
                0,
                GL_FALSE,
                0,
                GL_WRITE_ONLY,
                GL_RGBA16F
            );
        GL42.glBindImageTexture(
                1,
                reflection_history_[write_index],
                0,
                GL_FALSE,
                0,
                GL_WRITE_ONLY,
                GL_RGBA16F
            );
        GL42.glBindImageTexture(
                2,
                position_history_[write_index],
                0,
                GL_FALSE,
                0,
                GL_WRITE_ONLY,
                GL_RGBA16F
            );

        GL43.glDispatchCompute(
                static_cast<GLuint>((trace_width_ + 7) / 8),
                static_cast<GLuint>((trace_height_ + 7) / 8),
                1
            );

        GL42.glMemoryBarrier(
                GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                GL_TEXTURE_FETCH_BARRIER_BIT
            );
        GL20.glUseProgram(0);
        unbindTextureUnitsInline(7);

        history_index_ = write_index;
        history_valid_ = true;

        if (error)
        {
            error->clear();
        }

        return true;
    }

    /* Full-resolution AO/tone-map composite is independent from stochastic
     * tracing, so it can run only when direct lighting or Lumen history changed. */
    bool composite (
                const GBufferGpu& gbuffer,
                const DirectLightingGpu& direct,
                std::string *error = nullptr
        )
    {
        if (!ready()
                || !gbuffer.ready()
                || !direct.ready()
                || gbuffer.width() != width_
                || gbuffer.height() != height_)
        {
            setInlineError(error, "GPU Lumen composite resources are not ready");
            return false;
        }

        bindTextureUnitInline(0, gbuffer.positionDepthTexture());
        bindTextureUnitInline(1, gbuffer.normalRoughnessTexture());
        bindTextureUnitInline(2, direct.directTexture());
        bindTextureUnitInline(3, indirect_history_[history_index_]);
        bindTextureUnitInline(4, reflection_history_[history_index_]);

        GL20.glUseProgram(composite_program_);
        GL42.glBindImageTexture(
                0,
                final_color_,
                0,
                GL_FALSE,
                0,
                GL_WRITE_ONLY,
                GL_RGBA16F
            );

        GL43.glDispatchCompute(
                static_cast<GLuint>((width_ + 7) / 8),
                static_cast<GLuint>((height_ + 7) / 8),
                1
            );

        GL42.glMemoryBarrier(
                GL_SHADER_IMAGE_ACCESS_BARRIER_BIT |
                GL_TEXTURE_FETCH_BARRIER_BIT
            );
        GL20.glUseProgram(0);
        unbindTextureUnitsInline(5);

        if (error)
        {
            error->clear();
        }

        return true;
    }

    void shutdown ();

    bool ready () const;
    GLuint finalTexture () const { return final_color_; }
    GLuint indirectTexture () const { return indirect_history_[history_index_]; }
    GLuint reflectionTexture () const { return reflection_history_[history_index_]; }

private:
    static void bindTextureUnitInline (GLuint unit, GLuint texture)
    {
        GLModern.glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
        glBindTexture(GL_TEXTURE_2D, texture);
    }

    static void unbindTextureUnitsInline (GLuint count)
    {
        for (GLuint unit = 0; unit < count; ++unit)
        {
            bindTextureUnitInline(unit, 0);
        }

        GLModern.glActiveTexture(GL_TEXTURE0);
    }

    static void setInlineError (std::string *error, const char *message)
    {
        if (error)
        {
            *error = message ? message : "GPU Lumen error";
        }
    }

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
