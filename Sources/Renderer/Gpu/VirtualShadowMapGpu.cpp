#include "Renderer/Gpu/VirtualShadowMapGpu.hpp"

#include "Ecs/Ecs.hpp"
#include "Renderer/Gpu/GBufferGpu.hpp"
#include "Renderer/Gpu/Gpu.hpp"
#include "Renderer/Gpu/TriangleScene.hpp"
#include "Renderer/Gpu/VirtualShadowMapShader.hpp"

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
    const Math::Vec3 right = Math::normalize(Math::cross(reference, forward));
    const Math::Vec3 up = Math::normalize(Math::cross(forward, right));

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

} // namespace

bool VirtualShadowMapGpu::init (std::string *error)
{
    shutdown();

    if (!page_cache_.init(error))
    {
        return false;
    }

    begin_program_ = createComputeProgram(
            VIRTUAL_SHADOW_BEGIN_COMPUTE,
            error
        );
    if (begin_program_ == 0)
    {
        shutdown();
        return false;
    }

    const std::string mark_shader = virtualShadowMarkShader();
    mark_program_ = createComputeProgram(mark_shader.c_str(), error);
    if (mark_program_ == 0)
    {
        shutdown();
        return false;
    }

    mark_inverse_view_projection_location_ = GL20.glGetUniformLocation(
            mark_program_,
            "uGBufferInverseViewProjection"
        );
    mark_camera_location_ = GL20.glGetUniformLocation(
            mark_program_,
            "uCameraPosition"
        );
    mark_frame_location_ = GL20.glGetUniformLocation(
            mark_program_,
            "uFrameIndex"
        );
    mark_directional_light_location_ = GL20.glGetUniformLocation(
            mark_program_,
            "uDirectionalLightIndex"
        );
    mark_clipmap_count_location_ = GL20.glGetUniformLocation(
            mark_program_,
            "uShadowClipmapCount"
        );

    if (mark_inverse_view_projection_location_ < 0
            || mark_camera_location_ < 0
            || mark_frame_location_ < 0
            || mark_directional_light_location_ < 0
            || mark_clipmap_count_location_ < 0)
    {
        setError(error, "virtual shadow marking uniforms are unavailable");
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
            GL_DYNAMIC_DRAW
        );

    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, clipmap_buffer_);
    GL15.glBufferData(
            GL_SHADER_STORAGE_BUFFER,
            static_cast<LWCGLsizeiptr>(sizeof(clipmap_gpu_)),
            clipmap_gpu_.data(),
            GL_DYNAMIC_DRAW
        );

    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, shadow_light_buffer_);
    GL15.glBufferData(GL_SHADER_STORAGE_BUFFER, 16, nullptr, GL_DYNAMIC_DRAW);
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    shadow_light_capacity_ = 16u;

    if (error) error->clear();
    return true;
}

bool VirtualShadowMapGpu::resize (
            int width,
            int height,
            std::string *error
    )
{
    width_ = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;
    if (error) error->clear();
    return true;
}

bool VirtualShadowMapGpu::updateShadowLights (
            const Ecs::World& world,
            std::uint64_t scene_revision,
            std::string *error
    )
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
            shadow_lights_.size() * sizeof(ShadowLightGpu)
        );

    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, shadow_light_buffer_);
    if (required > shadow_light_capacity_)
    {
        std::size_t capacity = std::max<std::size_t>(256u, shadow_light_capacity_);
        while (capacity < required) capacity *= 2u;
        GL15.glBufferData(
                GL_SHADER_STORAGE_BUFFER,
                static_cast<LWCGLsizeiptr>(capacity),
                nullptr,
                GL_DYNAMIC_DRAW
            );
        shadow_light_capacity_ = capacity;
    }

    if (!shadow_lights_.empty())
    {
        GL15.glBufferSubData(
                GL_SHADER_STORAGE_BUFFER,
                0,
                static_cast<LWCGLsizeiptr>(
                        shadow_lights_.size() * sizeof(ShadowLightGpu)
                    ),
                shadow_lights_.data()
            );
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
                sizeof(target.view_projection)
            );
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
            clipmap_gpu_.data()
        );
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

bool VirtualShadowMapGpu::update (
            const Ecs::World& world,
            const GBufferGpu& gbuffer,
            const TriangleScene& triangles,
            const Math::Vec3& camera_position,
            std::uint64_t frame_index,
            std::uint64_t scene_revision,
            std::string *error
    )
{
    (void)triangles;

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
                    direction
                );
    }
    uploadClipmaps();

    page_cache_.beginFrame(frame_index);
    GL30.glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            9,
            page_cache_.metadataBuffer()
        );
    GL30.glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            10,
            page_cache_.pageTableBuffer()
        );
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, allocator_buffer_);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 13, clipmap_buffer_);

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
                gbuffer.inverseViewProjection().value
            );
        GL20.glUniform3f(
                mark_camera_location_,
                camera_position.x,
                camera_position.y,
                camera_position.z
            );
        GL20.glUniform1i(
                mark_frame_location_,
                static_cast<GLint>(frame_index & 0x7fffffffu)
            );
        GL20.glUniform1i(
                mark_directional_light_location_,
                directional_light_index
            );
        GL20.glUniform1i(mark_clipmap_count_location_, CLIPMAP_COUNT);
        GL43.glDispatchCompute(
                static_cast<GLuint>((width_ + 7) / 8),
                static_cast<GLuint>((height_ + 7) / 8),
                1
            );
        GL42.glMemoryBarrier(
                GL_SHADER_STORAGE_BARRIER_BIT |
                GL_TEXTURE_FETCH_BARRIER_BIT
            );
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    GL20.glUseProgram(0);
    if (error) error->clear();
    return true;
}

bool VirtualShadowMapGpu::bind (
            GLuint program,
            std::string *error
    ) const
{
    if (!ready() || program == 0)
    {
        setError(error, "virtual shadow resources are not bindable");
        return false;
    }

    GL30.glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            9,
            page_cache_.metadataBuffer()
        );
    GL30.glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            10,
            page_cache_.pageTableBuffer()
        );
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 11, shadow_light_buffer_);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 12, allocator_buffer_);
    GL30.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 13, clipmap_buffer_);

    if (error) error->clear();
    return true;
}

void VirtualShadowMapGpu::shutdown ()
{
    page_cache_.shutdown();
    if (allocator_buffer_ != 0) GL15.glDeleteBuffers(1, &allocator_buffer_);
    if (clipmap_buffer_ != 0) GL15.glDeleteBuffers(1, &clipmap_buffer_);
    if (shadow_light_buffer_ != 0) GL15.glDeleteBuffers(1, &shadow_light_buffer_);
    destroyProgram(&begin_program_);
    destroyProgram(&mark_program_);

    allocator_buffer_ = 0;
    clipmap_buffer_ = 0;
    shadow_light_buffer_ = 0;
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
    width_ = 0;
    height_ = 0;
}

} // namespace Gpu
} // namespace Renderer
