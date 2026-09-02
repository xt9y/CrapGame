#ifndef CRAPGAME_RENDERER_GPU_LUMENGPU_HPP
#define CRAPGAME_RENDERER_GPU_LUMENGPU_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Gpu/BvhShaders.hpp"
#include "Renderer/Gpu/DirectLightingGpu.hpp"
#include "Renderer/Gpu/GBufferGpu.hpp"
#include "Renderer/Gpu/Gpu.hpp"
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

        const bool use_bvh = direct.bvhReady();
        if (use_bvh && !ensureBvhTraceProgram(error))
        {
            return false;
        }

        const GLuint active_program = use_bvh ? bvh_trace_program_ : trace_program_;
        const GLint view_projection_location = use_bvh
            ? bvh_trace_view_projection_location_
            : trace_view_projection_location_;
        const GLint camera_location = use_bvh
            ? bvh_trace_camera_location_
            : trace_camera_location_;
        const GLint primitive_count_location = use_bvh
            ? bvh_trace_primitive_count_location_
            : trace_primitive_count_location_;
        const GLint frame_location = use_bvh
            ? bvh_trace_frame_location_
            : trace_frame_location_;
        const GLint history_valid_location = use_bvh
            ? bvh_trace_history_valid_location_
            : trace_history_valid_location_;

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

        GL20.glUseProgram(active_program);
        GL20.glUniformMatrix4fv(
                view_projection_location,
                1,
                GL_FALSE,
                view_projection.value
            );
        GL20.glUniform3f(
                camera_location,
                camera_position.x,
                camera_position.y,
                camera_position.z
            );
        GL20.glUniform1i(
                primitive_count_location,
                static_cast<GLint>(direct.primitiveCount())
            );
        GL20.glUniform1i(
                frame_location,
                static_cast<GLint>(frame_index & 0x7fffffffu)
            );
        GL20.glUniform1i(
                history_valid_location,
                history_valid_ ? 1 : 0
            );

        GL30.glBindBufferBase(
                GL_SHADER_STORAGE_BUFFER,
                7,
                direct.primitiveBuffer()
            );

        if (use_bvh)
        {
            GL20.glUniform1i(
                    bvh_trace_node_count_location_,
                    static_cast<GLint>(direct.bvhNodeCount())
                );
            GL30.glBindBufferBase(
                    GL_SHADER_STORAGE_BUFFER,
                    8,
                    direct.bvhNodeBuffer()
                );
            GL30.glBindBufferBase(
                    GL_SHADER_STORAGE_BUFFER,
                    9,
                    direct.bvhIndexBuffer()
                );
        }

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

    void releaseAcceleration ()
    {
        destroyProgram(&bvh_trace_program_);
        bvh_trace_view_projection_location_ = -1;
        bvh_trace_camera_location_ = -1;
        bvh_trace_primitive_count_location_ = -1;
        bvh_trace_node_count_location_ = -1;
        bvh_trace_frame_location_ = -1;
        bvh_trace_history_valid_location_ = -1;
    }

    bool ready () const;
    GLuint finalTexture () const { return final_color_; }
    GLuint indirectTexture () const { return indirect_history_[history_index_]; }
    GLuint reflectionTexture () const { return reflection_history_[history_index_]; }

private:
    bool ensureBvhTraceProgram (std::string *error)
    {
        if (bvh_trace_program_ != 0)
        {
            return true;
        }

        bvh_trace_program_ = createComputeProgram(LUMEN_TRACE_BVH_COMPUTE, error);
        if (bvh_trace_program_ == 0)
        {
            return false;
        }

        bvh_trace_view_projection_location_ = GL20.glGetUniformLocation(
                bvh_trace_program_, "uViewProjection"
            );
        bvh_trace_camera_location_ = GL20.glGetUniformLocation(
                bvh_trace_program_, "uCameraPosition"
            );
        bvh_trace_primitive_count_location_ = GL20.glGetUniformLocation(
                bvh_trace_program_, "uPrimitiveCount"
            );
        bvh_trace_node_count_location_ = GL20.glGetUniformLocation(
                bvh_trace_program_, "uBvhNodeCount"
            );
        bvh_trace_frame_location_ = GL20.glGetUniformLocation(
                bvh_trace_program_, "uFrameIndex"
            );
        bvh_trace_history_valid_location_ = GL20.glGetUniformLocation(
                bvh_trace_program_, "uHistoryValid"
            );

        if (bvh_trace_view_projection_location_ < 0
                || bvh_trace_camera_location_ < 0
                || bvh_trace_primitive_count_location_ < 0
                || bvh_trace_node_count_location_ < 0
                || bvh_trace_frame_location_ < 0
                || bvh_trace_history_valid_location_ < 0)
        {
            setInlineError(error, "GPU BVH Lumen uniforms are unavailable");
            destroyProgram(&bvh_trace_program_);
            return false;
        }

        return true;
    }

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
    GLuint bvh_trace_program_ = 0;
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
    GLint bvh_trace_view_projection_location_ = -1;
    GLint bvh_trace_camera_location_ = -1;
    GLint bvh_trace_primitive_count_location_ = -1;
    GLint bvh_trace_node_count_location_ = -1;
    GLint bvh_trace_frame_location_ = -1;
    GLint bvh_trace_history_valid_location_ = -1;

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
