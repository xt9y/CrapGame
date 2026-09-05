#ifndef CRAPGAME_RENDERER_GPU_LUMENGPU_HPP
#define CRAPGAME_RENDERER_GPU_LUMENGPU_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Gpu/DirectLightingGpu.hpp"
#include "Renderer/Gpu/DirtyTileGpu.hpp"
#include "Renderer/Gpu/FrameHotPath.hpp"
#include "Renderer/Gpu/GBufferGpu.hpp"
#include "Renderer/Gpu/Gpu.hpp"
#include "Renderer/Gpu/LumenImportedStageAShader.hpp"
#include "Renderer/Gpu/LumenStageAComposite.hpp"
#include "Renderer/Gpu/RadianceCacheGpu.hpp"
#include "Renderer/Gpu/ReflectionCacheGpu.hpp"
#include "Renderer/Gpu/ReprojectionCacheGpu.hpp"
#include "Renderer/Gpu/SurfaceFormats.hpp"
#include "Renderer/Gpu/TransparentGpu.hpp"
#include "Renderer/Math/Math.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cstddef>
#include <cstdint>
#include <exception>
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
        if (!ensureStageAImportedTraceShader(error)) return false;
        if (!ensureStageACompositeShader(error)) return false;
        if (!reprojection_cache_.ensure(width_, height_, error)) return false;
        if (!dirty_tile_gpu_.ensure((width_ + 1) / 2, (height_ + 1) / 2, error)) return false;
        return radiance_cache_.ensure(error);
    }

    bool traceShared (
                const GBufferGpu& gbuffer,
                const DirectLightingGpu& direct,
                const TriangleScene& triangles,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Math::Vec3& camera_position,
                std::uint64_t frame_index,
                bool secondary_refresh_due,
                std::string *error = nullptr
        );

    bool traceShared (
                const GBufferGpu& gbuffer,
                const DirectLightingGpu& direct,
                const TriangleScene& triangles,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Math::Vec3& camera_position,
                std::uint64_t frame_index,
                std::string *error = nullptr
        )
    {
        return traceShared(gbuffer,direct,triangles,view,projection,
                           camera_position,frame_index,true,error);
    }

    bool traceShared (
                const GBufferGpu& gbuffer,
                const DirectLightingGpu& direct,
                const Math::Mat4& view,
                const Math::Mat4& projection,
                const Math::Vec3& camera_position,
                std::uint64_t frame_index,
                bool secondary_refresh_due,
                std::string *error = nullptr
        )
    {
        return traceShared(gbuffer,direct,direct.triangleScene(),view,
                           projection,camera_position,frame_index,
                           secondary_refresh_due,error);
    }

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
        return traceShared(gbuffer,direct,direct.triangleScene(),view,
                           projection,camera_position,frame_index,true,error);
    }

    bool composite (
                const GBufferGpu& gbuffer,
                const DirectLightingGpu& direct,
                std::string *error = nullptr
        )
    {
        if (!ready() || !gbuffer.ready() || !direct.ready()
                || gbuffer.width() != width_ || gbuffer.height() != height_
                || composite_program_ != composite_stage_a_program_
                || composite_inverse_view_projection_location_ < 0)
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
        GL20.glUniformMatrix4fv(composite_inverse_view_projection_location_,1,GL_FALSE,
                                gbuffer.inverseViewProjection().value);
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
    const DirtyTileGpu& dirtyTiles () const { return dirty_tile_gpu_; }
    const RadianceCacheGpu& radianceCache () const { return radiance_cache_; }
    const ReflectionCacheGpu& reflectionCache () const { return reflection_cache_; }

private:
    bool ensureImportedTraceShader(std::string *error);

    bool ensureStageAImportedTraceShader(std::string *error)
    {
        if (trace_program_ != 0 && trace_program_ == trace_stage_a_program_
                && bvh_trace_active_)
            return true;

        GLuint candidate=0;
        try
        {
            const std::string source=lumenImportedStageATraceShader();
            candidate=createComputeProgram(source.c_str(),error);
        }
        catch(const std::exception& exception)
        {
            if(error)*error=exception.what();
            return false;
        }
        if(candidate==0)return false;

        const GLint view_projection=GL20.glGetUniformLocation(candidate,"uViewProjection");
        const GLint camera=GL20.glGetUniformLocation(candidate,"uCameraPosition");
        const GLint primitive_count=GL20.glGetUniformLocation(candidate,"uPrimitiveCount");
        const GLint bvh_count=GL20.glGetUniformLocation(candidate,"uBvhNodeCount");
        const GLint frame=GL20.glGetUniformLocation(candidate,"uFrameIndex");
        const GLint history=GL20.glGetUniformLocation(candidate,"uHistoryValid");
        const GLint imported_instances=GL20.glGetUniformLocation(candidate,"uImportedInstanceCount");
        const GLint imported_tlas=GL20.glGetUniformLocation(candidate,"uImportedTlasNodeCount");
        const GLint trace_materials=GL20.glGetUniformLocation(candidate,"uTraceMaterialCount");
        const GLint dirty_tile_dispatch=GL20.glGetUniformLocation(candidate,"uDirtyTileDispatch");
        const GLint radiance_generation=GL20.glGetUniformLocation(candidate,"uRadianceGeneration");
        const GLint light_count=GL20.glGetUniformLocation(candidate,"uLightCount");
        if(view_projection<0||camera<0||primitive_count<0||bvh_count<0||frame<0
                ||history<0||imported_instances<0||imported_tlas<0
                ||trace_materials<0||dirty_tile_dispatch<0||radiance_generation<0
                ||light_count<0)
        {
            destroyProgram(&candidate);
            setInlineError(error,"GPU Stage-A imported Lumen uniforms are unavailable");
            return false;
        }

        destroyProgram(&trace_program_);
        trace_program_=candidate;
        trace_stage_a_program_=candidate;
        trace_view_projection_location_=view_projection;
        trace_camera_location_=camera;
        trace_primitive_count_location_=primitive_count;
        trace_bvh_node_count_location_=bvh_count;
        trace_frame_location_=frame;
        trace_history_valid_location_=history;
        trace_imported_instance_count_location_=imported_instances;
        trace_imported_tlas_count_location_=imported_tlas;
        trace_material_count_location_=trace_materials;
        trace_dirty_tile_dispatch_location_=dirty_tile_dispatch;
        trace_radiance_generation_location_=radiance_generation;
        trace_light_count_location_=light_count;
        bvh_trace_active_=true;
        if(error)error->clear();
        return true;
    }

    bool ensureStageACompositeShader(std::string *error)
    {
        if(composite_program_!=0&&composite_program_==composite_stage_a_program_
                &&composite_inverse_view_projection_location_>=0)
            return true;
        GLuint candidate=createComputeProgram(LUMEN_STAGE_A_COMPOSITE_COMPUTE,error);
        if(candidate==0)return false;
        const GLint inverse_location=GL20.glGetUniformLocation(candidate,"uGBufferInverseViewProjection");
        if(inverse_location<0)
        {
            destroyProgram(&candidate);
            setInlineError(error,"GPU Stage-A Lumen composite uniforms are unavailable");
            return false;
        }
        destroyProgram(&composite_program_);
        composite_program_=candidate;
        composite_stage_a_program_=candidate;
        composite_inverse_view_projection_location_=inverse_location;
        if(error)error->clear();
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
    GLuint trace_stage_a_program_ = 0;
    GLuint composite_stage_a_program_ = 0;
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
    GLint trace_dirty_tile_dispatch_location_ = -1;
    GLint trace_radiance_generation_location_ = -1;
    GLint trace_light_count_location_ = -1;
    GLint composite_inverse_view_projection_location_ = -1;

    Math::Mat4 last_view_ = Math::identity();
    Math::Mat4 last_projection_ = Math::identity();
    Math::Vec3 last_camera_ = {0.0f, 0.0f, 0.0f};

    ReprojectionCacheGpu reprojection_cache_;
    DirtyTileGpu dirty_tile_gpu_;
    RadianceCacheGpu radiance_cache_;
    ReflectionCacheGpu reflection_cache_;
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
