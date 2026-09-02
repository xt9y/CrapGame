#include "Render.hpp"

#include <cmath>

namespace Renderer 
{
namespace 
{

Math::Vec3 toVec3 (const Ecs::Vec3& value) 
{
    return {value.x, value.y, value.z};
}

} // namespace

bool Rendering::init () 
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    glClearColor(0.055f, 0.070f, 0.105f, 1.0f);

    return true;
}

void Rendering::resize (int width, int height) 
{
    width_  = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;

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

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(projection_.value);
}

void Rendering::drawCube () const 
{
    constexpr float h = 0.75f;

    glBegin(GL_QUADS);

    glVertex3f(-h, -h,  h); glVertex3f( h, -h,  h); glVertex3f( h,  h,  h); glVertex3f(-h,  h,  h);
    glVertex3f( h, -h, -h); glVertex3f(-h, -h, -h); glVertex3f(-h,  h, -h); glVertex3f( h,  h, -h);
    glVertex3f(-h, -h, -h); glVertex3f(-h, -h,  h); glVertex3f(-h,  h,  h); glVertex3f(-h,  h, -h);
    glVertex3f( h, -h,  h); glVertex3f( h, -h, -h); glVertex3f( h,  h, -h); glVertex3f( h,  h,  h);
    glVertex3f(-h,  h,  h); glVertex3f( h,  h,  h); glVertex3f( h,  h, -h); glVertex3f(-h,  h, -h);
    glVertex3f(-h, -h, -h); glVertex3f( h, -h, -h); glVertex3f( h, -h,  h); glVertex3f(-h, -h,  h);

    glEnd();
}

void Rendering::drawPlane () const 
{
    glBegin(GL_QUADS);
    glVertex3f(-0.5f, 0.0f, -0.5f);
    glVertex3f(-0.5f, 0.0f,  0.5f);
    glVertex3f( 0.5f, 0.0f,  0.5f);
    glVertex3f( 0.5f, 0.0f, -0.5f);
    glEnd();
}

void Rendering::drawRenderable (
                const Ecs::TransformComponent& transform,
                const Ecs::RenderableComponent& renderable
        ) const 
{
    const Math::Mat4 model = 
        Math::transform(
                toVec3(transform.position),
                toVec3(transform.rotation),
                toVec3(transform.scale)
            );

    const Math::Mat4 model_view = 
        Math::multiply(view_, model);

    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(model_view.value);

    glColor3f(
            renderable.color.x, 
            renderable.color.y, 
            renderable.color.z
        );

    switch (renderable.primitive) 
    {
        case Ecs::Primitive::Cube:  drawCube(); break;
        case Ecs::Primitive::Plane: drawPlane(); break;
    }
}

void Rendering::render (const Ecs::World& world) 
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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

    applyCamera(*camera_transform, *camera);

    for (const Ecs::Entity entity : world.entities()) 
    {
        const Ecs::TransformComponent *transform = 
            world.getTransform(entity);

        const Ecs::RenderableComponent *renderable = 
            world.getRenderable(entity);

        if (!transform 
                || !renderable) 
        {
            continue;
        }

        drawRenderable(*transform, *renderable);
    }
}

void Rendering::shutdown () 
{
}

} // namespace Renderer
