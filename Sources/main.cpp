#include "Renderer/Render.hpp"
#include "Renderer/Test/TestScene.hpp"
#include "Ecs/Ecs.hpp"

#include <lwcgl/context.h>
#include <lwcgl/glmodern.h>

#include <chrono>
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

static int requestedFpsCap ()
{
    const char *value = std::getenv("CRAPGAME_FPS");

    if (!value || !*value)
    {
        return 0;
    }

    char *end = nullptr;
    const long parsed = std::strtol(value, &end, 10);

    if (end == value || *end != '\0' || parsed <= 0 || parsed > 100000)
    {
        std::fprintf(
                stderr,
                "Ignoring invalid CRAPGAME_FPS=%s; interactive rendering is uncapped\n",
                value
            );
        return 0;
    }

    return static_cast<int>(parsed);
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

    const int fps_cap = renderercheck_mode
        ? 0
        : requestedFpsCap();

    std::uint64_t frame = 0;
    int exit_code = 0;

    Display.setDisplayMode(
            new DisplayMode(window_width, window_height)
        );

#if !defined(__APPLE__)
    if (!renderercheck_mode)
    {
        /* LWJGL 2.9.3 exposes GL43. Interactive rendering requires it;
         * RendererCheck intentionally stays on the legacy CPU reference. */
        lwcglSetContextVersion(4, 3);
        lwcglSetContextProfile(LWCGL_CONTEXT_COMPATIBILITY_PROFILE);
    }
#endif

    if (Display.create() != 0)
    {
        ERROR("Display.create");
        return 2;
    }

#if !defined(__APPLE__)
    if (!renderercheck_mode && !lwcglModernGLAvailable())
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

    if (!renderercheck_mode)
    {
        /* Maximum-throughput mode is the default. CRAPGAME_FPS can add an
         * explicit software cap when a stable test cadence is desired. */
        Display.setVSyncEnabled(LWCGL_FALSE);

        if (fps_cap > 0)
        {
            std::fprintf(stderr, "Frame pacing: %d FPS cap, VSync off\n", fps_cap);
        }
        else
        {
            std::fprintf(stderr, "Frame pacing: uncapped, VSync off\n");
        }
    }

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

    using Clock = std::chrono::steady_clock;
    auto stats_start = Clock::now();
    std::uint64_t stats_frames = 0;

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
            if (fps_cap > 0)
            {
                Display.sync(fps_cap);
            }

            ++stats_frames;
            const auto now = Clock::now();
            const double elapsed = std::chrono::duration<double>(
                    now - stats_start
                ).count();

            if (elapsed >= 2.0)
            {
                const double fps = static_cast<double>(stats_frames) / elapsed;
                const double frame_ms = fps > 0.0 ? 1000.0 / fps : 0.0;

                std::fprintf(
                        stderr,
                        "CPU frame  %.3f ms  %.1f FPS  %dx%d\n",
                        frame_ms,
                        fps,
                        renderer_width,
                        renderer_height
                    );

                stats_start = now;
                stats_frames = 0;
            }
        }

        ++frame;
    }

    renderer.shutdown();
    Mouse.destroy();
    Keyboard.destroy();
    Display.destroy();
    return exit_code;
}