#include "Renderer/Render.hpp"
#include "Ecs/Ecs.hpp"

#include <cstdint>
#include <cstdio>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 870

static inline void ERROR (const char *operation) 
{
    const char *message = lwcglGetLastError();

    std::fprintf(
            stderr, "%s failed: %s\n", 
            operation, message 
            ? message : "unknown lwcgl error"
        );
}

static Ecs::Entity createScene (Ecs::World& world) 
{
    const Ecs::Entity cube   = world.createEntity(),
                      camera = world.createEntity(),
                      ground = world.createEntity();

    { // Camera
        world.addTransform(camera, {
            {0.0f, 3.0f, 8.0f},
            {-12.0f, 0.0f, 0.0f},
            {1.0f, 1.0f, 1.0f},
        });

        world.addCamera(camera, {
            60.0f, 0.1f, 100.0f, true
        });
    }

    { // Cube
        world.addTransform(cube, {
            {0.0f, 1.45f, 0.0f},
            {-35.2643897f, 0.0f, 45.0f},
            {1.0f, 1.0f, 1.0f},
        });

        world.addMesh(cube, {Ecs::MeshType::Cube});

        world.addRenderable(cube, {
            Ecs::Primitive::Cube,
            {1.0f, 1.0f, 1.0f},
        });
    }

    { // Ground
        world.addTransform(ground, {
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 0.0f},
            {14.0f, 1.0f, 14.0f},
        });

        world.addMesh(ground, {Ecs::MeshType::Plane});

        world.addRenderable(ground, {
            Ecs::Primitive::Plane,
            {0.28f, 0.30f, 0.34f},
        });
    }

    return cube;
}

int main () 
{
    const bool renderercheck_mode = 
        rendercheck_capture_requested() != 0;

    std::uint64_t frame = 0;
    int exit_code = 0;

    Display.setDisplayMode(
            new DisplayMode(WINDOW_WIDTH, WINDOW_HEIGHT)
        );

    Display.create();
    Keyboard.create();
    Mouse.create();

    Ecs::World world;

    const Ecs::Entity cube = 
        createScene(world);

    Renderer::Rendering renderer;
    renderer.init();

    while (!Display.isCloseRequested() 
            && !Keyboard.isKeyDown(Keyboard.KEY_ESCAPE)) 
    {
        renderer.resize(
                Display.getWidth(), 
                Display.getHeight()
            );

        renderer.render(world);
        glFinish();

        if (captureFrame(frame) != 0) 
        {
            std::fprintf(
                    stderr, 
                    "RendererCheck framebuffer capture failed\n"
                );
            
            exit_code = 2;
            break;
        }

        Display.update();

        Ecs::TransformComponent *cube_transform = 
            world.getTransform(cube);

        if (cube_transform) 
        {
            cube_transform->rotation.y += 0.65f;
            
            if (cube_transform->rotation.y >= 360.0f) 
            {
                cube_transform->rotation.y -= 360.0f;
            }
        }

        if (renderercheck_mode 
                && rendercheck_frame_is_last(frame)) 
        {
            break;
        }
        
        if (!renderercheck_mode) 
        {
            Display.sync(60); 
        }

        ++frame;
    }

    renderer.shutdown();
    Mouse.destroy();
    Keyboard.destroy();
    Display.destroy();
    return exit_code;
}
