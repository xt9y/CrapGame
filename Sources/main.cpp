#include "Renderer/Render.hpp"
#include "Renderer/Test/TestScene.hpp"
#include "Ecs/Ecs.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 870

#define TEST_WIDTH 320
#define TEST_HEIGHT 218

static inline void ERROR (const char *operation) 
{
    const char *message = lwcglGetLastError();

    std::fprintf(
            stderr, "%s failed: %s\n", 
            operation, message 
            ? message : "unknown lwcgl error"
        );
}

int main () 
{
    const bool renderercheck_mode = 
        rendercheck_capture_requested() != 0;

    const char *test_name = renderercheck_mode
        ? std::getenv("RENDERCHECK_TEST")
        : nullptr;

    if (renderercheck_mode
            && (!test_name
                || !*test_name
                || !Renderer::Test::knownTest(test_name)))
    {
        std::fprintf(
                stderr,
                "Unknown RendererCheck test: %s\n",
                test_name
                ? test_name
                : "(null)"
            );

        return 3;
    }

    const int window_width = renderercheck_mode
                ? TEST_WIDTH
                : WINDOW_WIDTH,
              window_height = renderercheck_mode
                ? TEST_HEIGHT
                : WINDOW_HEIGHT;

    std::uint64_t frame = 0;
    int exit_code = 0;

    Display.setDisplayMode(
            new DisplayMode(window_width, window_height)
        );

    Display.create();
    Keyboard.create();
    Mouse.create();

    Ecs::World world;
    Renderer::Test::SceneState scene_state;

    if (!Renderer::Test::buildScene(
            test_name,
            &world,
            &scene_state
        ))
    {
        std::fprintf(
                stderr,
                "Failed to create renderer test scene\n"
            );

        Mouse.destroy();
        Keyboard.destroy();
        Display.destroy();
        return 3;
    }

    Renderer::Rendering renderer;

    if (!renderer.init())
    {
        ERROR("renderer.init");

        Mouse.destroy();
        Keyboard.destroy();
        Display.destroy();
        return 2;
    }

    if (!renderer.setTestName(test_name))
    {
        std::fprintf(
                stderr,
                "Renderer has no output mode for test: %s\n",
                test_name
                ? test_name
                : "(null)"
            );

        renderer.shutdown();
        Mouse.destroy();
        Keyboard.destroy();
        Display.destroy();
        return 3;
    }

    while (!Display.isCloseRequested() 
            && !Keyboard.isKeyDown(Keyboard.KEY_ESCAPE)) 
    {
        renderer.resize(
                Display.getWidth(), 
                Display.getHeight()
            );

        renderer.render(world);
        glFinish();

        if (renderer.captureFrame(frame) != 0) 
        {
            std::fprintf(
                    stderr, 
                    "RendererCheck framebuffer capture failed\n"
                );
            
            exit_code = 2;
            break;
        }

        Display.update();

        Renderer::Test::updateScene(
                &world,
                &scene_state,
                frame
            );

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
