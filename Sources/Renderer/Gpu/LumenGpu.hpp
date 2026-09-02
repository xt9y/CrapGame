#ifndef CRAPGAME_RENDERER_GPU_LUMENGPU_HPP
#define CRAPGAME_RENDERER_GPU_LUMENGPU_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Gpu/BvhShadersV2.hpp"
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
        if (!ready() || !gbuffer.ready() || !direct.ready()
                || direct.primitiveBuffer() == 0
                || gbuffer.width() != width_ || gbuffer.height() != height_)
        {
            setInlineError(error, "GPU Lumen shared trace resources are not ready");
            return false;
        }

        const bool use_bvh = direct.bvhReady();
        const bool use_v2_shader = use_bvh || direct.benchmarkActive();
        if (!ensureBvhTraceShader(use_v2_shader, error))
        {
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
        GL20.glUniformMatrix4fv(trace_view_projection_location_, 1, GL_FALSE, view_projection.value);
        GL20.glUniform3f(trace_camera_location_, camera_position.x, camera_position.y, camera_position.z);
        GL20.glUniform1i(trace_primitive_count_location_, static_cast<GLint>(direct.primitiveCount()));
        GL20.glUniform1i(trace_frame_location_, static_cast<GLint>(frame_index & 0x7fffffffu));
        GL20.glUniform1i(trace_history_valid_location_, history_valid_ ? 1 : 0);

        GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, direct.primitiveBuffer());

        if (bvh_trace_active_)
        {
            GL20.glUniform1i(
                    trace_bvh_node_count_location_,
                    use_bvh ? static_cast<GLint>(direct.bvhNodeCount()) : 0
                );
            if (use_bvh)
            {
                GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, direct.bvhNodeBuffer());
            }
        }

        GL42.glBindImageTexture(0, indirect_history_[write_index], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        GL42.glBindImageTexture(1, reflection_history_[write_index], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        GL42.glBindImageTexture(2, position_history_[write_index], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        GL43.glDispatchCompute(
                static_cast<GLuint>((trace_width_ + 7) / 8),
                static_cast<GLuint>((trace_height_ + 7) / 8),
                1
            );
        GL42.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        GL20.glUseProgram(0);
        unbindTextureUnitsInline(7);

        history_index_ = write_index;
        history_valid_ = true;
        if (error) error->clear();
        return true;
    }

    bool composite (
                const GBufferGpu& gbuffer,
                const DirectLightingGpu& direct,
                std::string *error = nullptr
        )
    {
        if (!ready() || !gbuffer.ready() || !direct.ready()
                || gbuffer.width() != width_ || gbuffer.height() != height_)
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
        GL42.glBindImageTexture(0, final_color_, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
        GL43.glDispatchCompute(
                static_cast<GLuint>((width_ + 7) / 8),
                static_cast<GLuint>((height_ + 7) / 8),
                1
            );
        GL42.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
        GL20.glUseProgram(0);
        unbindTextureUnitsInline(5);
        if (error) error->clear();
        return true;
    }

    void shutdown ();

    bool ready () const;
    GLuint finalTexture () const { return final_color_; }
    GLuint indirectTexture () const { return indirect_history_[history_index_]; }
    GLuint reflectionTexture () const { return reflection_history_[history_index_]; }

private:
    bool queryBvhTraceLocations (GLuint program, GLint *view_projection, GLint *camera,
                GLint *primitive_count, GLint *node_count, GLint *frame, GLint *history) const
    {
        *view_projection = GL20.glGetUniformLocation(program, "uViewProjection");
        *camera = GL20.glGetUniformLocation(program, "uCameraPosition");
        *primitive_count = GL20.glGetUniformLocation(program, "uPrimitiveCount");
        *node_count = GL20.glGetUniformLocation(program, "uBvhNodeCount");
        *frame = GL20.glGetUniformLocation(program, "uFrameIndex");
        *history = GL20.glGetUniformLocation(program, "uHistoryValid");
        return *view_projection >= 0 && *camera >= 0 && *primitive_count >= 0
            && *node_count >= 0 && *frame >= 0 && *history >= 0;
    }

    bool ensureBvhTraceShader (bool activate, std::string *error)
    {
        if (bvh_trace_active_) return true;
        if (bvh_shader_validated_ && !activate) return true;

        GLuint candidate = createComputeProgram(LUMEN_TRACE_BVH_V2_COMPUTE, error);
        if (candidate == 0) return false;

        GLint view_projection = -1, camera = -1, primitive_count = -1,
              node_count = -1, frame = -1, history = -1;
        if (!queryBvhTraceLocations(candidate, &view_projection, &camera,
                &primitive_count, &node_count, &frame, &history))
        {
            destroyProgram(&candidate);
            setInlineError(error, "GPU BVH Lumen uniforms are unavailable");
            return false;
        }

        bvh_shader_validated_ = true;
        if (!activate)
        {
            destroyProgram(&candidate);
            if (error) error->clear();
            return true;
        }

        destroyProgram(&trace_program_);
        trace_program_ = candidate;
        trace_view_projection_location_ = view_projection;
        trace_camera_location_ = camera;
        trace_primitive_count_location_ = primitive_count;
        trace_bvh_node_count_location_ = node_count;
        trace_frame_location_ = frame;
        trace_history_valid_location_ = history;
        bvh_trace_active_ = true;
        if (error) error->clear();
        return true;
    }

    static void bindTextureUnitInline (GLuint unit, GLuint texture)
    {
        GLModern.glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
        glBindTexture(GL_TEXTURE_2D, texture);
    }

    static void unbindTextureUnitsInline (GLuint count)
    {
        for (GLuint unit = 0; unit < count; ++unit) bindTextureUnitInline(unit, 0);
        GLModern.glActiveTexture(GL_TEXTURE0);
    }

    static void setInlineError (std::string *error, const char *message)
    {
        if (error) *error = message ? message : "GPU Lumen error";
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
    GLint trace_bvh_node_count_location_ = -1;
    GLint trace_frame_location_ = -1;
    GLint trace_history_valid_location_ = -1;

    std::size_t primitive_capacity_ = 0;
    std::vector<PrimitiveGpu> primitives_;

    int history_index_ = 0;
    bool history_valid_ = false;
    bool bvh_shader_validated_ = false;
    bool bvh_trace_active_ = false;

    int width_ = 0;
    int height_ = 0;
    int trace_width_ = 0;
    int trace_height_ = 0;
};

} // namespace Gpu
} // namespace Renderer

#endif