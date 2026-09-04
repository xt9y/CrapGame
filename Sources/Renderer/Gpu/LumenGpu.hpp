#ifndef CRAPGAME_RENDERER_GPU_LUMENGPU_HPP
#define CRAPGAME_RENDERER_GPU_LUMENGPU_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Gpu/DirectLightingGpu.hpp"
#include "Renderer/Gpu/FrameHotPath.hpp"
#include "Renderer/Gpu/GBufferGpu.hpp"
#include "Renderer/Gpu/Gpu.hpp"
#include "Renderer/Gpu/ReprojectionCacheGpu.hpp"
#include "Renderer/Gpu/SurfaceFormats.hpp"
#include "Renderer/Gpu/TransparentGpu.hpp"
#include "Renderer/Math/Math.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace Renderer
{
namespace Gpu
{

class TriangleScene;

inline bool importedTraceSampleDue(
    bool camera_changed,
    std::uint64_t frame_index)
{
    return !camera_changed || frame_index % 3u == 0u;
}

class LumenGpu
{
public:
    bool init (std::string *error = nullptr);
    bool resize (int width, int height, std::string *error = nullptr);
    bool prewarmImportedTrace (std::string *error = nullptr)
    {
        if (!ensureImportedTraceShader(error)) return false;
        return reprojection_cache_.ensure(width_, height_, error);
    }

    bool traceShared (
                const GBufferGpu& gbuffer,
                const DirectLightingGpu& direct,
                const TriangleScene& triangles,
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
        return traceShared(gbuffer, direct, direct.triangleScene(), view,
                           projection, camera_position, frame_index, error);
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
        GL42.glBindImageTexture(0, final_color_, 0, GL_FALSE, 0, GL_WRITE_ONLY, imageFormatInline(LUMEN_FINAL_FORMAT));
        GL43.glDispatchCompute(
                static_cast<GLuint>((width_ + 7) / 8),
                static_cast<GLuint>((height_ + 7) / 8),
                1
            );
        GL42.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);

        if (conservativeGpuCleanupRequired(true))
        {
            GL20.glUseProgram(0);
            unbindTextureUnitsInline(5);
        }

        final_output_ = final_color_;
        if (const Ecs::World *world = direct.sceneWorld())
        {
            if (TransparentGpu *transparent = direct.transparentPass())
            {
                if (!transparent->render(
                        *world, gbuffer, direct, final_color_,
                        last_view_, last_projection_, last_camera_, error))
                {
                    return false;
                }
                final_output_ = transparent->finalTexture();
            }
        }

        if (error) error->clear();
        return true;
    }

    void shutdown ();

    bool ready () const;
    GLuint finalTexture () const
    {
        return final_output_ != 0 ? final_output_ : final_color_;
    }
    GLuint indirectTexture () const { return indirect_history_[history_index_]; }
    GLuint reflectionTexture () const { return reflection_history_[history_index_]; }
    const ReprojectionCacheGpu& reprojectionCache () const { return reprojection_cache_; }

private:
    bool ensureImportedTraceShader(std::string *error);

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

    static GLenum imageFormatInline (SurfaceFormat format)
    {
        return format == SurfaceFormat::Rgba8 ? GL_RGBA8 : GL_RGBA16F;
    }

    static void setInlineError (std::string *error, const char *message)
    {
        if (error) *error = message ? message : "GPU Lumen error";
    }

    void destroyTextures ();

    GLuint trace_program_ = 0;
    GLuint composite_program_ = 0;
    GLuint indirect_history_[2] = {0, 0};
    GLuint reflection_history_[2] = {0, 0};
    GLuint position_history_[2] = {0, 0};
    GLuint final_color_ = 0;
    GLuint final_output_ = 0;

    GLint trace_view_projection_location_ = -1;
    GLint trace_camera_location_ = -1;
    GLint trace_primitive_count_location_ = -1;
    GLint trace_bvh_node_count_location_ = -1;
    GLint trace_frame_location_ = -1;
    GLint trace_history_valid_location_ = -1;
    GLint trace_imported_instance_count_location_ = -1;
    GLint trace_imported_tlas_count_location_ = -1;
    GLint trace_material_count_location_ = -1;
    GLint trace_reprojection_available_location_ = -1;

    Math::Mat4 last_view_ = Math::identity();
    Math::Mat4 last_projection_ = Math::identity();
    Math::Vec3 last_camera_ = {0.0f, 0.0f, 0.0f};

    ReprojectionCacheGpu reprojection_cache_;
    std::uint64_t reprojection_scene_revision_ = 0u;
    std::uint64_t reprojection_mesh_revision_ = 0u;
    std::uint64_t reprojection_material_revision_ = 0u;
    int history_index_ = 0;
    bool history_valid_ = false;
    bool reprojection_scene_revision_valid_ = false;
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