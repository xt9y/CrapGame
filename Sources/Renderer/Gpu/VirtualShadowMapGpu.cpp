#include "Renderer/Gpu/VirtualShadowMapGpu.hpp"

#include "Ecs/Ecs.hpp"
#include "Renderer/Gpu/GBufferGpu.hpp"
#include "Renderer/Gpu/TriangleScene.hpp"

#include <algorithm>

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

    GL15.glGenBuffers(1, &shadow_light_buffer_);
    if (shadow_light_buffer_ == 0)
    {
        setError(error, "failed to allocate virtual-shadow light buffer");
        shutdown();
        return false;
    }

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
    bool directional = false;

    for (const Ecs::Entity entity : world.entities())
    {
        const Ecs::TransformComponent *transform = world.getTransform(entity);
        const Ecs::LightComponent *light = world.getLight(entity);
        if (!transform || !light
                || light->type != Ecs::LightType::Directional
                || light->intensity <= 0.0f
                || !light->casts_shadows)
        {
            continue;
        }

        direction = lightForward(*transform);
        directional = true;
        break;
    }

    for (int index = 0; index < CLIPMAP_COUNT; ++index)
    {
        const int level = VirtualShadowPolicy::FIRST_CLIPMAP_LEVEL + index;
        clipmaps_[static_cast<std::size_t>(index)] =
            directionalShadowClipmap(level, camera_position, direction);

        if (directional)
        {
            clipmaps_[static_cast<std::size_t>(index)].view_projection =
                clipmapViewProjection(
                        clipmaps_[static_cast<std::size_t>(index)],
                        direction
                    );
        }
    }

    page_cache_.beginFrame(frame_index);
    page_cache_.endFrame();
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
    GL30.glBindBufferBase(
            GL_SHADER_STORAGE_BUFFER,
            11,
            shadow_light_buffer_
        );

    if (error) error->clear();
    return true;
}

void VirtualShadowMapGpu::shutdown ()
{
    page_cache_.shutdown();
    if (shadow_light_buffer_ != 0)
    {
        GL15.glDeleteBuffers(1, &shadow_light_buffer_);
    }

    shadow_light_buffer_ = 0;
    shadow_light_capacity_ = 0u;
    shadow_light_revision_ = 0u;
    shadow_lights_.clear();
    clipmaps_ = {};
    width_ = 0;
    height_ = 0;
}

} // namespace Gpu
} // namespace Renderer
