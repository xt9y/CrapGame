#include "Renderer/Gpu/VirtualShadowMapGpu.hpp"

#include "Ecs/Ecs.hpp"
#include "Renderer/Gpu/GBufferGpu.hpp"
#include "Renderer/Gpu/Gpu.hpp"
#include "Renderer/Gpu/TriangleScene.hpp"
#include "Renderer/Gpu/VirtualShadowMapShader.hpp"
#include "Renderer/Gpu/VirtualShadowRasterShader.hpp"

#include <algorithm>
#include <cstring>

namespace Renderer
{
namespace Gpu
{
namespace
{

void setError (std::string *error, const char *message)
{
    if (error) *error = message ? message : "virtual shadow map error";
}

Math::Mat4 orthographic (
            float left,
            float right,
            float bottom,
            float top,
            float near_plane,
            float far_plane
    )
{
    Math::Mat4 result = {};
    result.value[0] = 2.0f / (right - left);
    result.value[5] = 2.0f / (top - bottom);
    result.value[10] = -2.0f / (far_plane - near_plane);
    result.value[12] = -(right + left) / (right - left);
    result.value[13] = -(top + bottom) / (top - bottom);
    result.value[14] = -(far_plane + near_plane) / (far_plane - near_plane);
    result.value[15] = 1.0f;
    return result;
}

Math::Vec3 lightForward (const Ecs::TransformComponent& transform)
{
    const Math::Vec3 rotation = {
        transform.rotation.x,
        transform.rotation.y,
        transform.rotation.z,
    };

    return Math::normalize(
            Math::transformDirection(
                    Math::rotationEuler(rotation),
                    {0.0f, 0.0f, -1.0f}
                )
        );
}

Math::Mat4 clipmapViewProjection (
            const VirtualShadowClipmap& clipmap,
            const Math::Vec3& light_direction
    )
{
    const Math::Vec3 forward = Math::normalize(light_direction);
    const Math::Vec3 reference = std::fabs(forward.y) > 0.95f
        ? Math::Vec3{0.0f, 0.0f, 1.0f}
        : Math::Vec3{0.0f, 1.0f, 0.0f};
    const Math::Vec3 up = Math::normalize(
            Math::cross(
                    forward,
                    Math::normalize(Math::cross(reference, forward))
                )
        );

    const float z_range = std::max(
            clipmap.extent * 2.0f,
            1.0f
        ) * VirtualShadowPolicy::Z_RANGE_SCALE;

    const Math::Vec3 eye = Math::subtract(
            clipmap.origin,
            Math::multiply(forward, z_range * 0.5f)
        );

    const Math::Mat4 view = Math::lookAt(eye, clipmap.origin, up);
    const Math::Mat4 projection = orthographic(
            -clipmap.extent,
            clipmap.extent,
            -clipmap.extent,
            clipmap.extent,
            0.1f,
            z_range
        );

    return Math::multiply(projection, view);
}

void deleteBuffer (GLuint *buffer)
{
    if (!buffer || *buffer == 0) return;
    GL15.glDeleteBuffers(1, buffer);
    *buffer = 0;
}

} // namespace

bool VirtualShadowMapGpu::init (std::string *error)
{
    shutdown();

    if (!page_cache_.init(error))
    {
        return false;
    }

    begin_program_ = createComputeProgram(VIRTUAL_SHADOW_BEGIN_COMPUTE, error);
    const std::string mark_shader = virtualShadowMarkShader();
    mark_program_ = begin_program_ != 0
        ? createComputeProgram(mark_shader.c_str(), error)
        : 0;
    raster_begin_program_ = mark_program_ != 0
        ? createComputeProgram(VIRTUAL_SHADOW_RASTER_BEGIN_COMPUTE, error)
        : 0;
    page_cull_program_ = raster_begin_program_ != 0
        ? createComputeProgram(VIRTUAL_SHADOW_PAGE_CULL_COMPUTE, error)
        : 0;
    clear_program_ = page_cull_program_ != 0
        ? createGraphicsProgram(
                VIRTUAL_SHADOW_CLEAR_VERTEX,
                VIRTUAL_SHADOW_CLEAR_FRAGMENT,
                error)
        : 0;
    raster_program_ = clear_program_ != 0
        ? createGraphicsProgram(
                VIRTUAL_SHADOW_RASTER_VERTEX,
                VIRTUAL_SHADOW_RASTER_FRAGMENT,
                error)
        : 0;
    finish_program_ = raster_program_ != 0
        ? createComputeProgram(VIRTUAL_SHADOW_FINISH_COMPUTE, error)
        : 0;

    if (finish_program_ == 0)
    {
        shutdown();
        return false;
    }

    mark_inverse_view_projection_location_ = GL20.glGetUniformLocation(
            mark_program_, "uGBufferInverseViewProjection");
    mark_camera_location_ = GL20.glGetUniformLocation(
            mark_program_, "uCameraPosition");
    mark_frame_location_ = GL20.glGetUniformLocation(
            mark_program_, "uFrameIndex");
    mark_directional_light_location_ = GL20.glGetUniformLocation(
            mark_program_, "uDirectionalLightIndex");
    mark_clipmap_count_location_ = GL20.glGetUniformLocation(
            mark_program_, "uShadowClipmapCount");
    page_cull_instance_count_location_ = GL20.glGetUniformLocation(
            page_cull_program_, "uImportedInstanceCount");
    page_cull_tlas_count_location_ = GL20.glGetUniformLocation(
            page_cull_program_, "uImportedTlasNodeCount");
    page_cull_clipmap_count_location_ = GL20.glGetUniformLocation(
            page_cull_program_, "uShadowClipmapCount");
    raster_material_count_location_ = GL20.glGetUniformLocation(
            raster_program_, "uTraceMaterialCount");

    if (mark_inverse_view_projection_location_ < 0
            || mark_camera_location_ < 0
            || mark_frame_location_ < 0
            || mark_directional_light_location_ < 0
            || mark_clipmap_count_location_ < 0
            || page_cull_instance_count_location_ < 0
            || page_cull_tlas_count_location_ < 0
            || page_cull_clipmap_count_location_ < 0
            || raster_material_count_location_ < 0)
    {
        setError(error, "virtual shadow shader uniforms are unavailable");
        shutdown();
        return false;
    }

    GL15.glGenBuffers(1, &allocator_buffer_);
    GL15.glGenBuffers(1, &clipmap_buffer_);
    GL15.glGenBuffers(1, &shadow_light_buffer_);
    if (allocator_buffer_ == 0
            || clipmap_buffer_ == 0
            || shadow_light_buffer_ == 0)
    {
        setError(error, "failed to allocate virtual-shadow buffers");
        shutdown();
        return false;
    }

    const std::uint32_t allocator[8] = {};
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, allocator_buffer_);
    GL15.glBufferData(
            GL_SHADER_STORAGE_BUFFER,
            static_cast<LWCGLsizeiptr>(sizeof(allocator)),
            allocator,
            GL_DYNAMIC_DRAW);

    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, clipmap_buffer_);
    GL15.glBufferData(
            GL_SHADER_STORAGE_BUFFER,
            static_cast<LWCGLsizeiptr>(sizeof(clipmap_gpu_)),
            clipmap_gpu_.data(),
            GL_DYNAMIC_DRAW);

    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, shadow_light_buffer_);
    GL15.glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_DRAW);
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    shadow_light_capacity_ = 16u;

    if (!createRasterResources(error))
    {
        shutdown();
        return false;
    }

    if (error) error->clear();
    return true;
}

bool VirtualShadowMapGpu::createRasterResources (std::string *error)
{
    GL15.glGenBuffers(1, &dirty_page_buffer_);
    GL15.glGenBuffers(1, &raster_triangle_buffer_);
    GL15.glGenBuffers(1, &raster_command_buffer_);
    GL30.glGenFramebuffers(1, &framebuffer_);
    GL30.glGenVertexArrays(1, &raster_vao_);
    depth_atlas_ = lwcgl_glGenTexture();

    if (dirty_page_buffer_ == 0
            || raster_triangle_buffer_ == 0
            || raster_command_buffer_ == 0
            || framebuffer_ == 0
            || raster_vao_ == 0
            || depth_atlas_ == 0)
    {
        setError(error, "failed to allocate virtual-shadow raster resources");
        return false;
    }

    const LWCGLsizeiptr dirty_bytes = static_cast<LWCGLsizeiptr>(
            16u + static_cast<std::size_t>(VirtualShadowPolicy::MAX_PHYSICAL_PAGES) * 16u);
    const LWCGLsizeiptr triangle_bytes = static_cast<LWCGLsizeiptr>(
            static_cast<std::size_t>(VirtualShadowPolicy::MAX_RASTER_TRIANGLES) * 16u);
    const LWCGLsizeiptr command_bytes = static_cast<LWCGLsizeiptr>(
            16u + static_cast<std::size_t>(VirtualShadowPolicy::MAX_PHYSICAL_PAGES) * 16u);

    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, dirty_page_buffer_);
    GL15.glBufferData(GL_SHADER_STORAGE_BUFFER, dirty_bytes, nullptr, GL_DYNAMIC_DRAW);
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, raster_triangle_buffer_);
    GL15.glBufferData(GL_SHADER_STORAGE_BUFFER, triangle_bytes, nullptr, GL_DYNAMIC_DRAW);
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, raster_command_buffer_);
    GL15.glBufferData(GL_SHADER_STORAGE_BUFFER, command_bytes, nullptr, GL_DYNAMIC_DRAW);
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    glBindTexture(GL_TEXTURE_2D, depth_atlas_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_DEPTH_COMPONENT24,
            VirtualShadowPolicy::ATLAS_WIDTH,
            VirtualShadowPolicy::ATLAS_HEIGHT,
            0,
            GL_DEPTH_COMPONENT,
            GL_UNSIGNED_INT,
            nullptr);

    GL30.glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    GL30.glFramebufferTexture2D(
            GL_FRAMEBUFFER,
            GL_DEPTH_ATTACHMENT,
            GL_TEXTURE_2D,
            depth_atlas_,
            0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    const GLenum status = GL30.glCheckFramebufferStatus(GL_FRAMEBUFFER);
    GL30.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        setError(error, "virtual-shadow physical atlas framebuffer is incomplete");
        return false;
    }

    return true;
}

bool VirtualShadowMapGpu::resize (
            int width,
            int height,
            std::string *error)
{
    width_ = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;
    if (error) error->clear();
    return true;
}

bool VirtualShadowMapGpu::updateShadowLights (
            const Ecs::World& world,
            std::uint64_t scene_revision,
            std::string *error)
{
    if (shadow_light_revision_ == scene_revision)
    {
        if (error) error->clear();
        return true;
    }

    shadow_lights_.clear();
    for (const Ecs::Entity entity : world.entities())
    {
        const Ecs::TransformComponent *transform = world.getTransform(entity);
        const Ecs::LightComponent *light = world.getLight(entity);
        if (!transform || !light || light->intensity <= 0.0f)
        {
            continue;
        }

        ShadowLightGpu gpu = {};
        gpu.source_shape[0] = light->source_radius;
        gpu.source_shape[1] = Math::radians(light->source_angle);
        gpu.source_shape[2] = light->type == Ecs::LightType::Directional
            ? 0.0f
            : (light->type == Ecs::LightType::Point ? 1.0f : 2.0f);
        gpu.source_shape[3] = light->casts_shadows ? 1.0f : 0.0f;
        shadow_lights_.push_back(gpu);
    }

    const std::size_t required = std::max<std::size_t>(
            16u,
            shadow_lights_.size() * sizeof(ShadowLightGpu));

    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, shadow_light_buffer_);
    if (required > shadow_light_capacity_)
    {
        std::size_t capacity = std::max<std::size_t>(256u, shadow_light_capacity_);
        while (capacity < required) capacity *= 2u;
        GL15.glBufferData(
                GL_SHADER_STORAGE_BUFFER,
                static_cast<LWCGLsizeiptr>(capacity),
                nullptr,
                GL_DYNAMIC_DRAW);
        shadow_light_capacity_ = capacity;
    }

    if (!shadow_lights_.empty())
    {
        GL15.glBufferSubData(
                GL_SHADER_STORAGE_BUFFER,
                0,
                static_cast<LWCGLsizeiptr>(
                        shadow_lights_.size() * sizeof(ShadowLightGpu)),
                shadow_lights_.data());
    }
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    shadow_light_revision_ = scene_revision;
    if (error) error->clear();
    return true;
}

void VirtualShadowMapGpu::uploadClipmaps ()
{
    for (std::size_t index = 0; index < clipmaps_.size(); ++index)
    {
        const VirtualShadowClipmap& source = clipmaps_[index];
        ClipmapGpu& target = clipmap_gpu_[index];
        std::memcpy(
                target.view_projection,
                source.view_projection.value,
                sizeof(target.view_projection));
        target.origin_extent[0] = source.origin.x;
        target.origin_extent[1] = source.origin.y;
        target.origin_extent[2] = source.origin.z;
        target.origin_extent[3] = source.extent;
        target.page_offset_level[0] = source.page_offset_x;
        target.page_offset_level[1] = source.page_offset_y;
        target.page_offset_level[2] = source.level;
        target.page_offset_level[3] = 0;
        target.parameters[0] = source.texel_world_size;
        target.parameters[1] = source.texel_world_size *
            static_cast<float>(VirtualShadowPolicy::PAGE_SIZE);
        target.parameters[2] = 0.0f;
        target.parameters[3] = 0.0f;
    }

    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, clipmap_buffer_);
    GL15.glBufferSubData(
            GL_SHADER_STORAGE_BUFFER,
            0,
            static_cast<LWCGLsizeiptr>(sizeof(clipmap_gpu_)),
            clipmap_gpu_.data());
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

bool VirtualShadowMapGpu::renderDirtyPages (
            const TriangleScene& triangles,
            std::string *error)
{
    if (!triangles.ready())
    {
        if (error) error->clear();
        return true;
    }

    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, raster_command_buffer_);
    GL20.glUseProgram(raster_begin_program_);
    GL43.glDispatchCompute(1, 1, 1);
    GL42.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, triangles.meshBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, triangles.instanceBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, triangles.tlasNodeBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, dirty_page_buffer_);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, clipmap_buffer_);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, raster_triangle_buffer_);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, raster_command_buffer_);
    GL20.glUseProgram(page_cull_program_);
    GL20.glUniform1i(
            page_cull_instance_count_location_,
            static_cast<GLint>(triangles.instanceCount()));
    GL20.glUniform1i(
            page_cull_tlas_count_location_,
            static_cast<GLint>(triangles.tlasNodeCount()));
    GL20.glUniform1i(page_cull_clipmap_count_location_, CLIPMAP_COUNT);
    GL43.glDispatchCompute(VirtualShadowPolicy::MAX_PHYSICAL_PAGES, 1, 1);
    GL42.glMemoryBarrier(
            GL_SHADER_STORAGE_BARRIER_BIT |
            GL_COMMAND_BARRIER_BIT);

    GL30.glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glViewport(
            0,
            0,
            VirtualShadowPolicy::ATLAS_WIDTH,
            VirtualShadowPolicy::ATLAS_HEIGHT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    GL30.glBindVertexArray(raster_vao_);

    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, dirty_page_buffer_);
    GL20.glUseProgram(clear_program_);
    glDepthFunc(GL_ALWAYS);
    GL31.glDrawArraysInstanced(
            GL_TRIANGLES,
            0,
            6,
            VirtualShadowPolicy::MAX_PHYSICAL_PAGES);

    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, raster_triangle_buffer_);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, triangles.triangleBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, triangles.instanceBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, clipmap_buffer_);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, triangles.traceRecordBuffer());
    GLModern.glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, triangles.colorAtlas());
    GLModern.glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, triangles.dataAtlas());
    GL20.glUseProgram(raster_program_);
    GL20.glUniform1i(
            raster_material_count_location_,
            static_cast<GLint>(triangles.traceMaterialCount()));
    glDepthFunc(GL_LESS);
    GL15.glBindBuffer(GL_DRAW_INDIRECT_BUFFER, raster_command_buffer_);
    GL43.glMultiDrawArraysIndirect(
            GL_TRIANGLES,
            reinterpret_cast<const void*>(16),
            VirtualShadowPolicy::MAX_PHYSICAL_PAGES,
            16);
    GL15.glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

    GL20.glUseProgram(0);
    GL30.glBindVertexArray(0);
    GL30.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width_, height_);
    GLModern.glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    GLModern.glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    GL42.glMemoryBarrier(
            GL_FRAMEBUFFER_BARRIER_BIT |
            GL_TEXTURE_FETCH_BARRIER_BIT);

    GL30.glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            0,
            page_cache_.metadataBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, dirty_page_buffer_);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, allocator_buffer_);
    GL20.glUseProgram(finish_program_);
    GL43.glDispatchCompute(
            static_cast<GLuint>(
                    (VirtualShadowPolicy::MAX_PHYSICAL_PAGES + 63) / 64),
            1,
            1);
    GL42.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    GL20.glUseProgram(0);

    if (error) error->clear();
    return true;
}

bool VirtualShadowMapGpu::update (
            const Ecs::World& world,
            const GBufferGpu& gbuffer,
            const TriangleScene& triangles,
            const Math::Vec3& camera_position,
            std::uint64_t frame_index,
            std::uint64_t scene_revision,
            std::string *error)
{
    if (!ready() || !gbuffer.ready()
            || gbuffer.width() != width_
            || gbuffer.height() != height_)
    {
        setError(error, "virtual shadow resources are not ready");
        return false;
    }

    if (!updateShadowLights(world, scene_revision, error))
    {
        return false;
    }

    Math::Vec3 direction = {0.0f, -1.0f, 0.0f};
    int directional_light_index = -1,
        active_light_index = 0;

    for (const Ecs::Entity entity : world.entities())
    {
        const Ecs::TransformComponent *transform = world.getTransform(entity);
        const Ecs::LightComponent *light = world.getLight(entity);
        if (!transform || !light || light->intensity <= 0.0f)
        {
            continue;
        }

        if (directional_light_index < 0
                && light->type == Ecs::LightType::Directional
                && light->casts_shadows)
        {
            direction = lightForward(*transform);
            directional_light_index = active_light_index;
        }
        ++active_light_index;
    }

    for (int index = 0; index < CLIPMAP_COUNT; ++index)
    {
        const int level = VirtualShadowPolicy::FIRST_CLIPMAP_LEVEL + index;
        clipmaps_[static_cast<std::size_t>(index)] =
            directionalShadowClipmap(level, camera_position, direction);
        clipmaps_[static_cast<std::size_t>(index)].view_projection =
            clipmapViewProjection(
                    clipmaps_[static_cast<std::size_t>(index)],
                    direction);
    }
    uploadClipmaps();

    page_cache_.beginFrame(frame_index);
    GL30.glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            0,
            page_cache_.metadataBuffer());
    GL30.glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            1,
            page_cache_.pageTableBuffer());
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, allocator_buffer_);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, clipmap_buffer_);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, dirty_page_buffer_);

    GL20.glUseProgram(begin_program_);
    GL43.glDispatchCompute(1, 1, 1);
    GL42.glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    if (directional_light_index >= 0)
    {
        GLModern.glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gbuffer.depthTexture());
        GL20.glUseProgram(mark_program_);
        GL20.glUniformMatrix4fv(
                mark_inverse_view_projection_location_,
                1,
                GL_FALSE,
                gbuffer.inverseViewProjection().value);
        GL20.glUniform3f(
                mark_camera_location_,
                camera_position.x,
                camera_position.y,
                camera_position.z);
        GL20.glUniform1i(
                mark_frame_location_,
                static_cast<GLint>(frame_index & 0x7fffffffu));
        GL20.glUniform1i(
                mark_directional_light_location_,
                directional_light_index);
        GL20.glUniform1i(mark_clipmap_count_location_, CLIPMAP_COUNT);
        GL43.glDispatchCompute(
                static_cast<GLuint>((width_ + 7) / 8),
                static_cast<GLuint>((height_ + 7) / 8),
                1);
        GL42.glMemoryBarrier(
                GL_SHADER_STORAGE_BARRIER_BIT |
                GL_TEXTURE_FETCH_BARRIER_BIT);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    GL20.glUseProgram(0);
    if (!renderDirtyPages(triangles, error))
    {
        return false;
    }

    if (error) error->clear();
    return true;
}

bool VirtualShadowMapGpu::bind (
            GLuint program,
            std::string *error) const
{
    if (!ready() || program == 0)
    {
        setError(error, "virtual shadow resources are not bindable");
        return false;
    }

    /* Direct-light programs already consume the OpenGL 4.3 minimum SSBO
     * binding range through TriangleScene. Shadow projection is performed in
     * its own compute pass, so do not stack VSM SSBO bindings onto that layout. */
    GLModern.glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, depth_atlas_);
    GLModern.glActiveTexture(GL_TEXTURE0);

    if (error) error->clear();
    return true;
}

void VirtualShadowMapGpu::shutdown ()
{
    page_cache_.shutdown();
    deleteBuffer(&allocator_buffer_);
    deleteBuffer(&clipmap_buffer_);
    deleteBuffer(&shadow_light_buffer_);
    deleteBuffer(&dirty_page_buffer_);
    deleteBuffer(&raster_triangle_buffer_);
    deleteBuffer(&raster_command_buffer_);

    if (depth_atlas_ != 0) glDeleteTextures(depth_atlas_);
    if (framebuffer_ != 0) GL30.glDeleteFramebuffers(1, &framebuffer_);
    if (raster_vao_ != 0) GL30.glDeleteVertexArrays(1, &raster_vao_);
    depth_atlas_ = 0;
    framebuffer_ = 0;
    raster_vao_ = 0;

    destroyProgram(&begin_program_);
    destroyProgram(&mark_program_);
    destroyProgram(&raster_begin_program_);
    destroyProgram(&page_cull_program_);
    destroyProgram(&clear_program_);
    destroyProgram(&raster_program_);
    destroyProgram(&finish_program_);

    shadow_light_capacity_ = 0u;
    shadow_light_revision_ = 0u;
    shadow_lights_.clear();
    clipmaps_ = {};
    clipmap_gpu_ = {};
    mark_inverse_view_projection_location_ = -1;
    mark_camera_location_ = -1;
    mark_frame_location_ = -1;
    mark_directional_light_location_ = -1;
    mark_clipmap_count_location_ = -1;
    page_cull_instance_count_location_ = -1;
    page_cull_tlas_count_location_ = -1;
    page_cull_clipmap_count_location_ = -1;
    raster_material_count_location_ = -1;
    width_ = 0;
    height_ = 0;
}

} // namespace Gpu
} // namespace Renderer
