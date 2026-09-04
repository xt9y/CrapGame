#include "Renderer/Gpu/LumenGpu.hpp"
#include "Renderer/Gpu/LumenImportedShader.hpp"
#include "Renderer/Gpu/TriangleScene.hpp"
#include "Renderer/Material/Material.hpp"
#include "Renderer/Mesh/Mesh.hpp"

#include <cmath>
#include <exception>
#include <string>

#ifndef GL_SHADER_STORAGE_BARRIER_BIT
#define GL_SHADER_STORAGE_BARRIER_BIT 0x00002000
#endif

namespace Renderer { namespace Gpu {
namespace {

bool matrixChanged(const Math::Mat4& a, const Math::Mat4& b)
{
    for (int index = 0; index < 16; ++index)
    {
        if (std::fabs(a.value[index] - b.value[index]) > 1.0e-5f)
            return true;
    }
    return false;
}

} // namespace

bool LumenGpu::ensureImportedTraceShader(std::string *error)
{
    if (bvh_trace_active_) return true;

    GLuint candidate = 0;
    try
    {
        const std::string source = lumenImportedTraceShader();
        candidate = createComputeProgram(source.c_str(), error);
    }
    catch (const std::exception& exception)
    {
        if (error) *error = exception.what();
        return false;
    }
    if (candidate == 0) return false;

    const GLint view_projection = GL20.glGetUniformLocation(candidate, "uViewProjection");
    const GLint camera = GL20.glGetUniformLocation(candidate, "uCameraPosition");
    const GLint primitive_count = GL20.glGetUniformLocation(candidate, "uPrimitiveCount");
    const GLint bvh_count = GL20.glGetUniformLocation(candidate, "uBvhNodeCount");
    const GLint frame = GL20.glGetUniformLocation(candidate, "uFrameIndex");
    const GLint history = GL20.glGetUniformLocation(candidate, "uHistoryValid");
    const GLint imported_instances = GL20.glGetUniformLocation(candidate, "uImportedInstanceCount");
    const GLint imported_tlas = GL20.glGetUniformLocation(candidate, "uImportedTlasNodeCount");
    const GLint trace_materials = GL20.glGetUniformLocation(candidate, "uTraceMaterialCount");
    const GLint dirty_tile_dispatch = GL20.glGetUniformLocation(candidate, "uDirtyTileDispatch");
    const GLint radiance_generation = GL20.glGetUniformLocation(candidate, "uRadianceGeneration");

    if (view_projection < 0 || camera < 0 || primitive_count < 0
            || bvh_count < 0 || frame < 0 || history < 0
            || imported_instances < 0 || imported_tlas < 0
            || trace_materials < 0 || dirty_tile_dispatch < 0
            || radiance_generation < 0)
    {
        destroyProgram(&candidate);
        setInlineError(error, "GPU imported Lumen uniforms are unavailable");
        return false;
    }

    destroyProgram(&trace_program_);
    trace_program_ = candidate;
    trace_view_projection_location_ = view_projection;
    trace_camera_location_ = camera;
    trace_primitive_count_location_ = primitive_count;
    trace_bvh_node_count_location_ = bvh_count;
    trace_frame_location_ = frame;
    trace_history_valid_location_ = history;
    trace_imported_instance_count_location_ = imported_instances;
    trace_imported_tlas_count_location_ = imported_tlas;
    trace_material_count_location_ = trace_materials;
    trace_dirty_tile_dispatch_location_ = dirty_tile_dispatch;
    trace_radiance_generation_location_ = radiance_generation;
    bvh_trace_active_ = true;
    if (error) error->clear();
    return true;
}

bool LumenGpu::traceShared(
            const GBufferGpu& gbuffer,
            const DirectLightingGpu& direct,
            const TriangleScene& triangles,
            const Math::Mat4& view,
            const Math::Mat4& projection,
            const Math::Vec3& camera_position,
            std::uint64_t frame_index,
            std::string *error)
{
    if (!ready() || !gbuffer.ready() || !direct.ready() || !triangles.ready()
            || direct.primitiveBuffer() == 0
            || gbuffer.width() != width_ || gbuffer.height() != height_)
    {
        setInlineError(error, "GPU Lumen imported trace resources are not ready");
        return false;
    }
    if (!ensureImportedTraceShader(error)) return false;
    if (!reprojection_cache_.ensure(width_, height_, error)) return false;
    if (!dirty_tile_gpu_.ensure(trace_width_, trace_height_, error)) return false;
    if (!radiance_cache_.ensure(error)) return false;

    if (!history_valid_)
    {
        reprojection_cache_.invalidate();
        reprojection_scene_revision_valid_ = false;
    }

    const bool camera_changed = matrixChanged(view, last_view_)
        || matrixChanged(projection, last_projection_);
    const std::uint64_t scene_revision = direct.sceneRevision();
    const std::uint64_t mesh_revision = Mesh::loadedMeshRevision();
    const std::uint64_t material_revision = Material::revision();

    RevisionState radiance_revisions = {};
    radiance_revisions.geometry = scene_revision;
    radiance_revisions.material = scene_revision;
    radiance_revisions.lighting = scene_revision;
    radiance_revisions.mesh_registry = mesh_revision;
    radiance_revisions.material_registry = material_revision;
    if (!radiance_cache_.updateGeneration(radiance_revisions,error)) return false;

    const bool scene_unchanged = reprojection_scene_revision_valid_
        && scene_revision == reprojection_scene_revision_
        && mesh_revision == reprojection_mesh_revision_
        && material_revision == reprojection_material_revision_;
    const bool reprojection_active = camera_changed && scene_unchanged
        && history_valid_ && reprojection_cache_.historyValid();
    const bool temporal_history_valid = history_valid_ && scene_unchanged
        && !camera_changed;
    const bool reflection_history_valid = history_valid_ && scene_unchanged;

    last_view_ = view;
    last_projection_ = projection;
    last_camera_ = camera_position;
    final_output_ = final_color_;

    if (history_valid_ && !reprojection_active
            && !importedTraceSampleDue(camera_changed, frame_index))
    {
        if (error) error->clear();
        return true;
    }

    const Math::Mat4 view_projection = Math::multiply(projection, view);
    const int read_index = history_index_;
    const int write_index = 1 - history_index_;

    if (reprojection_active)
    {
        if (!reprojection_cache_.reproject(
                gbuffer,
                indirect_history_[read_index],
                reflection_history_[read_index],
                indirect_history_[write_index],
                reflection_history_[write_index],
                position_history_[write_index],
                camera_position,
                error))
            return false;
        if (!dirty_tile_gpu_.compact(reprojection_cache_.validMaskTexture(), error))
            return false;
    }

    bindTextureUnitInline(0, gbuffer.positionDepthTexture());
    bindTextureUnitInline(1, gbuffer.normalRoughnessTexture());
    bindTextureUnitInline(2, gbuffer.albedoMetallicTexture());
    bindTextureUnitInline(3, direct.directTexture());
    bindTextureUnitInline(4, indirect_history_[read_index]);
    bindTextureUnitInline(5, reflection_history_[read_index]);
    bindTextureUnitInline(6, position_history_[read_index]);
    GLModern.glActiveTexture(GL_TEXTURE0 + 7);
    glBindTexture(GL_TEXTURE_2D_ARRAY, triangles.colorAtlas());
    GLModern.glActiveTexture(GL_TEXTURE0 + 8);
    glBindTexture(GL_TEXTURE_2D_ARRAY, triangles.dataAtlas());
    bindTextureUnitInline(9, gbuffer.specularIorTexture());
    bindTextureUnitInline(10, gbuffer.advancedMaterialTexture());
    bindTextureUnitInline(14, gbuffer.materialIdentityTexture());
    GLModern.glActiveTexture(GL_TEXTURE0);

    GL20.glUseProgram(trace_program_);
    GL20.glUniformMatrix4fv(trace_view_projection_location_, 1, GL_FALSE,
                            view_projection.value);
    GL20.glUniform3f(trace_camera_location_, camera_position.x,
                     camera_position.y, camera_position.z);
    GL20.glUniform1i(trace_primitive_count_location_,
                     static_cast<GLint>(direct.primitiveCount()));
    GL20.glUniform1i(trace_bvh_node_count_location_,
                     direct.bvhReady()
                         ? static_cast<GLint>(direct.bvhNodeCount()) : 0);
    GL20.glUniform1i(trace_frame_location_,
                     static_cast<GLint>(frame_index & 0x7fffffffu));
    GL20.glUniform1i(trace_history_valid_location_, temporal_history_valid ? 1 : 0);
    GL20.glUniform1i(trace_imported_instance_count_location_,
                     static_cast<GLint>(triangles.instanceCount()));
    GL20.glUniform1i(trace_imported_tlas_count_location_,
                     static_cast<GLint>(triangles.tlasNodeCount()));
    GL20.glUniform1i(trace_material_count_location_,
                     static_cast<GLint>(triangles.traceMaterialCount()));
    GL20.glUniform1i(trace_dirty_tile_dispatch_location_, reprojection_active ? 1 : 0);
    GL20.glUniform1i(trace_radiance_generation_location_,
                     static_cast<GLint>(radiance_cache_.generation()));
    if (!reflection_cache_.bind(trace_program_,reprojection_cache_,
                                reflection_history_[read_index],
                                reflection_history_valid,error))
        return false;

    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, triangles.triangleBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, triangles.meshBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, triangles.instanceBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, triangles.blasNodeBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, triangles.tlasNodeBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, direct.primitiveBuffer());
    if (direct.bvhReady())
        GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, direct.bvhNodeBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, triangles.traceRecordBuffer());
    if (reprojection_active) dirty_tile_gpu_.bindTiles(8);
    if (!radiance_cache_.bind(static_cast<GLuint>(RadianceCachePolicy::BUFFER_BINDING),error))
        return false;

    GL42.glBindImageTexture(0, indirect_history_[write_index], 0, GL_FALSE, 0,
                            GL_WRITE_ONLY, imageFormatInline(LUMEN_HISTORY_FORMAT));
    GL42.glBindImageTexture(1, reflection_history_[write_index], 0, GL_FALSE, 0,
                            GL_WRITE_ONLY, imageFormatInline(LUMEN_HISTORY_FORMAT));
    GL42.glBindImageTexture(2, position_history_[write_index], 0, GL_FALSE, 0,
                            GL_WRITE_ONLY, imageFormatInline(LUMEN_POSITION_HISTORY_FORMAT));
    if (reprojection_active)
        dirty_tile_gpu_.dispatchIndirect();
    else
        GL43.glDispatchCompute(static_cast<GLuint>((trace_width_ + 7) / 8),
                               static_cast<GLuint>((trace_height_ + 7) / 8), 1);
    GL42.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
                         | GL_TEXTURE_FETCH_BARRIER_BIT
                         | GL_SHADER_STORAGE_BARRIER_BIT);

    if (conservativeGpuCleanupRequired(true))
    {
        GL20.glUseProgram(0);
        for (GLuint unit = 0; unit < 15; ++unit)
        {
            GLModern.glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(unit == 7 || unit == 8
                          ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D, 0);
        }
        GLModern.glActiveTexture(GL_TEXTURE0);
        GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, 0);
        GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER,
                              static_cast<GLuint>(RadianceCachePolicy::BUFFER_BINDING),0);
    }

    if (!reprojection_cache_.capture(gbuffer, view_projection, error)) return false;

    history_index_ = write_index;
    history_valid_ = true;
    reprojection_scene_revision_ = scene_revision;
    reprojection_mesh_revision_ = mesh_revision;
    reprojection_material_revision_ = material_revision;
    reprojection_scene_revision_valid_ = true;
    if (error) error->clear();
    return true;
}

} }
