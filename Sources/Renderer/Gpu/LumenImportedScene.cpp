#include "Renderer/Gpu/LumenGpu.hpp"
#include "Renderer/Gpu/LumenImportedShader.hpp"
#include "Renderer/Gpu/TriangleScene.hpp"

#include <exception>
#include <string>

namespace Renderer { namespace Gpu {

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

    if (view_projection < 0 || camera < 0 || primitive_count < 0
            || bvh_count < 0 || frame < 0 || history < 0
            || imported_instances < 0 || imported_tlas < 0
            || trace_materials < 0)
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

    last_view_ = view;
    last_projection_ = projection;
    last_camera_ = camera_position;
    final_output_ = final_color_;

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
    GLModern.glActiveTexture(GL_TEXTURE0 + 7);
    glBindTexture(GL_TEXTURE_2D_ARRAY, triangles.colorAtlas());
    GLModern.glActiveTexture(GL_TEXTURE0 + 8);
    glBindTexture(GL_TEXTURE_2D_ARRAY, triangles.dataAtlas());
    bindTextureUnitInline(9, gbuffer.specularIorTexture());
    bindTextureUnitInline(10, gbuffer.advancedMaterialTexture());
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
    GL20.glUniform1i(trace_history_valid_location_, history_valid_ ? 1 : 0);
    GL20.glUniform1i(trace_imported_instance_count_location_,
                     static_cast<GLint>(triangles.instanceCount()));
    GL20.glUniform1i(trace_imported_tlas_count_location_,
                     static_cast<GLint>(triangles.tlasNodeCount()));
    GL20.glUniform1i(trace_material_count_location_,
                     static_cast<GLint>(triangles.traceMaterialCount()));

    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, triangles.triangleBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, triangles.meshBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, triangles.instanceBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, triangles.blasNodeBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, triangles.tlasNodeBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, direct.primitiveBuffer());
    if (direct.bvhReady())
        GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, direct.bvhNodeBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, triangles.traceRecordBuffer());

    GL42.glBindImageTexture(0, indirect_history_[write_index], 0, GL_FALSE, 0,
                            GL_WRITE_ONLY, imageFormatInline(LUMEN_HISTORY_FORMAT));
    GL42.glBindImageTexture(1, reflection_history_[write_index], 0, GL_FALSE, 0,
                            GL_WRITE_ONLY, imageFormatInline(LUMEN_HISTORY_FORMAT));
    GL42.glBindImageTexture(2, position_history_[write_index], 0, GL_FALSE, 0,
                            GL_WRITE_ONLY, imageFormatInline(LUMEN_POSITION_HISTORY_FORMAT));
    GL43.glDispatchCompute(static_cast<GLuint>((trace_width_ + 7) / 8),
                           static_cast<GLuint>((trace_height_ + 7) / 8), 1);
    GL42.glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
                         | GL_TEXTURE_FETCH_BARRIER_BIT);

    if (conservativeGpuCleanupRequired(true))
    {
        GL20.glUseProgram(0);
        for (GLuint unit = 0; unit < 11; ++unit)
        {
            GLModern.glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(unit == 7 || unit == 8
                          ? GL_TEXTURE_2D_ARRAY : GL_TEXTURE_2D, 0);
        }
        GLModern.glActiveTexture(GL_TEXTURE0);
    }

    history_index_ = write_index;
    history_valid_ = true;
    if (error) error->clear();
    return true;
}

} }
