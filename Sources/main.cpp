#include "Renderer/Render.hpp"
#include "Renderer/Test/TestScene.hpp"
#include "Ecs/Ecs.hpp"

#include <lwcgl/context.h>
#include <lwcgl/glmodern.h>

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

#if !defined(__APPLE__)
    /*
     * LWJGL 2.9.3 exposes GL43. Request a compatibility context so the
     * existing GL11 path remains valid while heavy passes migrate to the GPU.
     */
    lwcglSetContextVersion(4, 3);
    lwcglSetContextProfile(LWCGL_CONTEXT_COMPATIBILITY_PROFILE);
#endif

    if (Display.create() != 0)
    {
        ERROR("Display.create");
        return 2;
    }

#if !defined(__APPLE__)
    if (!lwcglModernGLAvailable())
    {
        std::fprintf(
                stderr,
                "CrapGame requires OpenGL 4.3 for the GPU renderer "
                "(got %d.%d; missing %s)\n",
                lwcglModernGLMajorVersion(),
                lwcglModernGLMinorVersion(),
                lwcglModernGLMissingFunction()
                    ? lwcglModernGLMissingFunction()
                    : "required GL43 capability"
            );
        Display.destroy();
        return 2;
    }
#endif

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

    /* Select the output path before init so RendererCheck stays on the
     * deterministic CPU reference while normal gameplay initializes GL43. */
    if (!renderer.setTestName(test_name))
    {
        std::fprintf(
                stderr,
                "Renderer has no output mode for test: %s\n",
                test_name
                ? test_name
                : "(null)"
            );

        Mouse.destroy();
        Keyboard.destroy();
        Display.destroy();
        return 3;
    }

    if (!renderer.init())
    {
        ERROR("renderer.init");

        Mouse.destroy();
        Keyboard.destroy();
        Display.destroy();
        return 2;
    }

    int renderer_width = Display.getWidth(),
        renderer_height = Display.getHeight();

    renderer.resize(renderer_width, renderer_height);

    while (!Display.isCloseRequested() 
            && !Keyboard.isKeyDown(Keyboard.KEY_ESCAPE)) 
    {
        const int display_width = Display.getWidth(),
                  display_height = Display.getHeight();

        if (display_width != renderer_width
                || display_height != renderer_height)
        {
            renderer_width = display_width;
            renderer_height = display_height;
            renderer.resize(renderer_width, renderer_height);
        }

        renderer.render(world);

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