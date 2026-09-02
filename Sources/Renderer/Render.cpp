#include "Render.hpp"

#include "Renderer/Lighting/Lighting.hpp"
#include "Renderer/Lumen/SceneLighting.hpp"
#include "Renderer/Mesh/Mesh.hpp"
#include "Renderer/Shadows/Shadows.hpp"

#include <algorithm>
#include <cmath>

namespace Renderer 
{
namespace 
{

Math::Vec3 toVec3 (const Ecs::Vec3& value) 
{
    return {value.x, value.y, value.z};
}

std::uint8_t toByte (float value) 
{
    return static_cast<std::uint8_t>(
            Math::saturate(value) * 255.0f + 0.5f
        );
}

struct ActiveLight 
{
    const Ecs::TransformComponent *transform;
    const Ecs::LightComponent *light;
};

} // namespace

bool Rendering::init () 
{
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    glClearColor(0.055f, 0.070f, 0.105f, 1.0f);

    return true;
}

void Rendering::resize (int width, int height) 
{
    width_  = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;

    const std::size_t pixel_count = 
        static_cast<std::size_t>(width_) *
        static_cast<std::size_t>(height_);

    frame_color_.resize(pixel_count);
    resolved_color_.resize(pixel_count);
    color_buffer_.resize(pixel_count * 3u);
    present_buffer_.resize(pixel_count * 3u);

    gbuffer_.resize(width_, height_);
    history_.resize(width_, height_);

    glViewport(0, 0, width_, height_);
}

void Rendering::applyCamera (
                const Ecs::TransformComponent& transform,
                const Ecs::CameraComponent& camera
        ) 
{
    const float aspect = 
        static_cast<float>(width_) /
        static_cast<float>(height_);

    const Math::Vec3 position = 
        toVec3(transform.position);

    const Math::Vec3 rotation = 
        toVec3(transform.rotation);

    const float pitch = Math::radians(rotation.x),
                yaw   = Math::radians(rotation.y);

    const Math::Vec3 forward = 
        Math::normalize({
            std::cos(pitch) * std::sin(yaw),
            std::sin(pitch),
            -std::cos(pitch) * std::cos(yaw),
        });

    projection_ = Math::perspective(
            camera.fov_degrees,
            aspect,
            camera.near_plane,
            camera.far_plane
        );

    view_ = Math::lookAt(
            position,
            Math::add(position, forward),
            {0.0f, 1.0f, 0.0f}
        );
}

void Rendering::renderGeometry (const Ecs::World& world) 
{
    gbuffer_.clear();

    for (const Ecs::Entity entity : world.entities()) 
    {
        const Ecs::TransformComponent *transform = 
            world.getTransform(entity);

        const Ecs::MeshComponent *mesh = 
            world.getMesh(entity);

        const Ecs::RenderableComponent *renderable = 
            world.getRenderable(entity);

        const Ecs::MaterialComponent *material = 
            world.getMaterial(entity);

        if (!transform 
                || !mesh 
                || !renderable 
                || !renderable->visible 
                || !material) 
        {
            continue;
        }

        gbuffer_.rasterize(
                entity,
                Mesh::meshForType(mesh->mesh),
                *transform,
                *material,
                view_,
                projection_
            );
    }
}

void Rendering::composeLighting (
                const Ecs::World& world,
                const Math::Vec3& camera_position
        ) 
{
    const Math::Vec3 clear_color = {
        0.055f, 0.070f, 0.105f
    };

    std::vector<ActiveLight> lights;

    for (const Ecs::Entity entity : world.entities()) 
    {
        const Ecs::TransformComponent *transform = 
            world.getTransform(entity);

        const Ecs::LightComponent *light = 
            world.getLight(entity);

        if (!transform 
                || !light) 
        {
            continue;
        }

        lights.push_back({transform, light});
    }

    for (int y = 0; y < height_; ++y) 
    {
        for (int x = 0; x < width_; ++x) 
        {
            const GBuffer::Pixel& pixel = 
                gbuffer_.pixel(x, y);

            Math::Vec3 color = clear_color;

            if (pixel.valid) 
            {
                color = pixel.emissive;

                for (const ActiveLight& active_light : lights) 
                {
                    const Lighting::LightSample light_sample = 
                        Lighting::sampleLight(
                                *active_light.light,
                                *active_light.transform,
                                pixel.world_position
                            );

                    float visibility = 1.0f;

                    if (active_light.light->casts_shadows
                            && light_sample.valid) 
                    {
                        visibility = shadows_.visibility(
                                pixel.world_position,
                                pixel.normal,
                                light_sample
                            );
                    }

                    color = Math::add(
                            color,
                            Lighting::evaluateDirect(
                                    pixel,
                                    camera_position,
                                    light_sample,
                                    visibility
                                )
                        );
                }

                color = Lighting::toneMap(color);
            }

            const std::size_t pixel_index =
                static_cast<std::size_t>(y) *
                static_cast<std::size_t>(width_) +
                static_cast<std::size_t>(x);

            frame_color_[pixel_index] = color;
        }
    }
}

void Rendering::writeColorBuffer (const std::vector<Math::Vec3>& color) 
{
    const std::size_t pixel_count =
        static_cast<std::size_t>(width_) *
        static_cast<std::size_t>(height_);

    if (color.size() != pixel_count) 
    {
        return;
    }

    for (std::size_t index = 0; index < pixel_count; ++index) 
    {
        const std::size_t offset = index * 3u;

        color_buffer_[offset + 0u] = toByte(color[index].x);
        color_buffer_[offset + 1u] = toByte(color[index].y);
        color_buffer_[offset + 2u] = toByte(color[index].z);
    }
}

void Rendering::present () 
{
    const std::size_t row_bytes = 
        static_cast<std::size_t>(width_) * 3u;

    for (int y = 0; y < height_; ++y) 
    {
        const std::size_t source = 
            static_cast<std::size_t>(height_ - 1 - y) * row_bytes;

        const std::size_t destination = 
            static_cast<std::size_t>(y) * row_bytes;

        std::copy_n(
                color_buffer_.data() + source,
                row_bytes,
                present_buffer_.data() + destination
            );
    }

    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glRasterPos2f(-1.0f, -1.0f);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glDrawPixels(
            width_, height_,
            GL_RGB, GL_UNSIGNED_BYTE,
            present_buffer_.data()
        );
}

void Rendering::render (const Ecs::World& world) 
{
    const Ecs::Entity camera_entity = 
        world.activeCamera();

    if (camera_entity == Ecs::INVALID_ENTITY) 
    {
        return;
    }

    const Ecs::TransformComponent *camera_transform = 
        world.getTransform(camera_entity);

    const Ecs::CameraComponent *camera = 
        world.getCamera(camera_entity);

    if (!camera_transform 
            || !camera) 
    {
        return;
    }

    const Math::Vec3 camera_position =
        toVec3(camera_transform->position);

    applyCamera(*camera_transform, *camera);
    renderGeometry(world);

    Temporal::calculateMotion(
            &gbuffer_,
            world,
            frame_state_
        );

    tracer_.build(world, camera_position);

    cards_.build(world);
    surface_cache_.build(world, cards_);

    shadows_.build(world);

    Lumen::updateSceneLighting(
            &surface_cache_,
            world,
            shadows_
        );

    radiance_cache_.update(
            gbuffer_,
            view_,
            projection_,
            tracer_,
            surface_cache_,
            camera_position,
            frame_state_.frameIndex(),
            12u
        );

    composeLighting(world, camera_position);

    Temporal::resolveTaa(
            gbuffer_,
            history_,
            frame_color_,
            &resolved_color_
        );

    writeColorBuffer(resolved_color_);
    present();

    history_.store(gbuffer_, resolved_color_);
    frame_state_.capture(world, view_, projection_);
}

void Rendering::shutdown () 
{
    frame_color_.clear();
    resolved_color_.clear();
    color_buffer_.clear();
    present_buffer_.clear();
    history_.clear();
}

} // namespace Renderer
