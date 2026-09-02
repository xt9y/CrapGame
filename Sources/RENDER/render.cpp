#include "render.hpp"

#include <lwcgl/lwcgl.h>

#include <cmath>

namespace render {
namespace {

#define PI (3.14159265358979323846f)
#define DEG_TO_RAD (PI / 180.0f);

} // namespace

bool Renderer::init () 
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);

    glClearColor(0.055f, 0.070f, 0.105f, 1.0f);

    return true;
}

void Renderer::resize (int width, int height) 
{
    width_  = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;

    glViewport(0, 0, width_, height_);
}

void Renderer::applyCamera (const ecs::TransformComponent& transform,
                            const ecs::CameraComponent& camera) const 
{
    const float aspect = 
        static_cast<float>(width_) / 
        static_cast<float>(height_);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(camera.fov_degrees, aspect, camera.near_plane, camera.far_plane);

    const float pitch       = transform.rotation.x * DEG_TO_RAD;
    const float yaw         = transform.rotation.y * DEG_TO_RAD;
    const float cos_pitch   = std::cos(pitch);

    const ecs::Vec3 forward = 
    {
        cos_pitch * std::sin(yaw),
        std::sin(pitch),
        -cos_pitch * std::cos(yaw),
    };

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gluLookAt(
        transform.position.x,
        transform.position.y,
        transform.position.z,
        transform.position.x + forward.x,
        transform.position.y + forward.y,
        transform.position.z + forward.z,
        0.0,
        1.0,
        0.0
    );
}

void Renderer::drawCube () const 
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

void Renderer::drawPlane () const 
{
    glBegin(GL_QUADS);
    glVertex3f(-0.5f, 0.0f, -0.5f);
    glVertex3f(-0.5f, 0.0f,  0.5f);
    glVertex3f( 0.5f, 0.0f,  0.5f);
    glVertex3f( 0.5f, 0.0f, -0.5f);
    glEnd();
}

void Renderer::drawRenderable (const ecs::TransformComponent& transform,
                               const ecs::RenderableComponent& renderable) const 
{
    glPushMatrix();
    glTranslatef(transform.position.x, transform.position.y, transform.position.z);

    // Y first keeps a cube body diagonal vertical while it spins around world up.
    glRotatef(transform.rotation.y, 0.0f, 1.0f, 0.0f);
    glRotatef(transform.rotation.x, 1.0f, 0.0f, 0.0f);
    glRotatef(transform.rotation.z, 0.0f, 0.0f, 1.0f);

    glScalef(transform.scale.x, transform.scale.y, transform.scale.z);
    glColor3f(renderable.color.x, renderable.color.y, renderable.color.z);

    switch (renderable.primitive) 
    {
        case ecs::Primitive::Cube:  drawCube(); break;
        case ecs::Primitive::Plane: drawPlane(); break;
    }

    glPopMatrix();
}

void Renderer::render (const ecs::World& world) 
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const ecs::Entity camera_entity = world.activeCamera();
    if (camera_entity == ecs::INVALID_ENTITY) return;

    const ecs::TransformComponent* camera_transform = world.getTransform(camera_entity);
    const ecs::CameraComponent* camera = world.getCamera(camera_entity);
    if (!camera_transform || !camera) return;

    applyCamera(*camera_transform, *camera);

    for (const ecs::Entity entity : world.entities()) {
        const ecs::TransformComponent* transform = world.getTransform(entity);
        const ecs::RenderableComponent* renderable = world.getRenderable(entity);
        if (!transform || !renderable) continue;
        drawRenderable(*transform, *renderable);
    }
}

void Renderer::shutdown () {
}

} // namespace render
