#include "Camera.hpp"

#include <lwcgl/lwcgl.h>

#include <algorithm>
#include <cmath>

namespace
{

constexpr float MOVE_SPEED = 5.0f;
constexpr float FAST_MULTIPLIER = 4.0f;
constexpr float MOUSE_SENSITIVITY = 0.12f;
constexpr float MAX_PITCH = 89.0f;
constexpr float PI = 3.14159265358979323846f;

bool keyDown (int key)
{
    return Keyboard.isKeyDown && Keyboard.isKeyDown(key) != 0;
}

float radians (float degrees)
{
    return degrees * PI / 180.0f;
}

float lengthSquared (const Ecs::Vec3& value)
{
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

void add (Ecs::Vec3 *value, const Ecs::Vec3& direction, float amount)
{
    value->x += direction.x * amount;
    value->y += direction.y * amount;
    value->z += direction.z * amount;
}

} // namespace

void Camera::update (Ecs::World& world, float delta_seconds)
{
    if (!Keyboard.isCreated || !Mouse.isCreated
            || !Keyboard.isCreated() || !Mouse.isCreated())
    {
        return;
    }

    if (!initialized_)
    {
        grabbed_ = true;
        if (Mouse.setGrabbed)
        {
            Mouse.setGrabbed(LWCGL_TRUE);
        }
        if (Mouse.getDX)
        {
            (void)Mouse.getDX();
        }
        if (Mouse.getDY)
        {
            (void)Mouse.getDY();
        }
        initialized_ = true;
    }

    const bool tab = keyDown(Keyboard.KEY_TAB);

    if (tab && !tab_down_)
    {
        grabbed_ = !grabbed_;

        if (Mouse.setGrabbed)
        {
            Mouse.setGrabbed(grabbed_ ? LWCGL_TRUE : LWCGL_FALSE);
        }

        if (grabbed_)
        {
            if (Mouse.getDX)
            {
                (void)Mouse.getDX();
            }
            if (Mouse.getDY)
            {
                (void)Mouse.getDY();
            }
        }
    }

    tab_down_ = tab;

    int mouse_dx = 0,
        mouse_dy = 0;

    if (grabbed_)
    {
        if (Mouse.getDX)
        {
            mouse_dx = Mouse.getDX();
        }
        if (Mouse.getDY)
        {
            mouse_dy = Mouse.getDY();
        }
    }

    const float forward_input =
        (keyDown(Keyboard.KEY_W) ? 1.0f : 0.0f)
        - (keyDown(Keyboard.KEY_S) ? 1.0f : 0.0f);

    const float right_input =
        (keyDown(Keyboard.KEY_D) ? 1.0f : 0.0f)
        - (keyDown(Keyboard.KEY_A) ? 1.0f : 0.0f);

    const bool look_changed = mouse_dx != 0 || mouse_dy != 0;
    const bool movement_requested =
        forward_input != 0.0f
        || right_input != 0.0f;

    if (!look_changed && !movement_requested)
    {
        return;
    }

    const Ecs::Entity entity = world.activeCamera();

    if (entity == Ecs::INVALID_ENTITY)
    {
        return;
    }

    const Ecs::World& read_world = world;
    const Ecs::TransformComponent *current = read_world.getTransform(entity);

    if (!current)
    {
        return;
    }

    Ecs::TransformComponent next = *current;
    bool changed = false;

    if (look_changed)
    {
        next.rotation.y +=
            static_cast<float>(mouse_dx) * MOUSE_SENSITIVITY;

        next.rotation.x = std::max(
                -MAX_PITCH,
                std::min(
                        MAX_PITCH,
                        next.rotation.x
                            + static_cast<float>(mouse_dy) * MOUSE_SENSITIVITY
                    )
            );

        changed = true;
    }

    const float delta = std::max(0.0f, delta_seconds);

    if (movement_requested && delta > 0.0f)
    {
        const float pitch = radians(next.rotation.x),
                    yaw = radians(next.rotation.y);

        const Ecs::Vec3 forward = {
            std::cos(pitch) * std::sin(yaw),
            std::sin(pitch),
            -std::cos(pitch) * std::cos(yaw),
        };

        const Ecs::Vec3 right = {
            std::cos(yaw),
            0.0f,
            std::sin(yaw),
        };

        Ecs::Vec3 movement = {0.0f, 0.0f, 0.0f};
        add(&movement, forward, forward_input);
        add(&movement, right, right_input);

        const float movement_length_squared = lengthSquared(movement);

        if (movement_length_squared > 0.000001f)
        {
            const float inverse_length =
                1.0f / std::sqrt(movement_length_squared);

            const bool fast =
                keyDown(Keyboard.KEY_LSHIFT)
                || keyDown(Keyboard.KEY_RSHIFT);

            const float distance =
                MOVE_SPEED
                * (fast ? FAST_MULTIPLIER : 1.0f)
                * delta;

            next.position.x += movement.x * inverse_length * distance;
            next.position.y += movement.y * inverse_length * distance;
            next.position.z += movement.z * inverse_length * distance;
            changed = true;
        }
    }

    if (!changed)
    {
        return;
    }

    if (Ecs::TransformComponent *transform = world.getTransform(entity))
    {
        *transform = next;
    }
}
