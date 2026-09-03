#include "Renderer/Render.hpp"
#include "Renderer/PerformanceMetrics.hpp"
#include "Renderer/Test/TestScene.hpp"
#include "Ecs/Ecs.hpp"

#include <lwcgl/context.h>
#include <lwcgl/glmodern.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

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

    const bool performance_mode =
        Renderer::PerformanceMetrics::requested();

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

    const int fps_cap = renderercheck_mode || performance_mode
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
        /* LWJGL 2.9.3 exposes GL43. Interactive and performance rendering
         * require it; visual RendererCheck intentionally stays on the legacy
         * deterministic CPU reference. */
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
        /* Rendering and simulation are intentionally independent. Rendering
         * runs as fast as possible by default, while world updates are fixed
         * at 60 Hz below. */
        Display.setVSyncEnabled(LWCGL_FALSE);

        if (performance_mode)
        {
            std::fprintf(
                    stderr,
                    "RendererCheck perf: uncapped, VSync off, %.0f ms warmup, %.0f ms total\n",
                    Renderer::PerformanceMetrics::warmupMilliseconds(),
                    Renderer::PerformanceMetrics::durationMilliseconds()
                );
        }
        else if (fps_cap > 0)
        {
            std::fprintf(stderr, "Frame pacing: %d FPS cap, VSync off\n", fps_cap);
        }
        else
        {
            std::fprintf(stderr, "Frame pacing: uncapped, VSync off\n");
        }

        if (!performance_mode)
        {
            std::fprintf(stderr, "Simulation: fixed 60 Hz, render-rate independent\n");
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

    /* Select the output path before init so visual RendererCheck stays on the
     * deterministic CPU reference while gameplay/perf initializes GL43. */
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
    constexpr double SIMULATION_STEP_SECONDS = 1.0 / 60.0;
    constexpr double MAX_SIMULATION_DELTA_SECONDS = 0.25;

    auto stats_start = Clock::now();
    auto simulation_previous = stats_start;
    const auto performance_start = stats_start;
    const double performance_warmup_ms =
        Renderer::PerformanceMetrics::warmupMilliseconds();
    const double performance_duration_ms =
        Renderer::PerformanceMetrics::durationMilliseconds();

    double simulation_accumulator = 0.0;
    std::uint64_t simulation_tick = 0;
    std::uint64_t stats_frames = 0;
    std::vector<double> cpu_frame_samples;

    if (performance_mode)
    {
        cpu_frame_samples.reserve(16384u);
    }

    while (!Display.isCloseRequested() 
            && !Keyboard.isKeyDown(Keyboard.KEY_ESCAPE)) 
    {
        const auto frame_started = Clock::now();

        if (!renderercheck_mode)
        {
            const auto simulation_now = frame_started;
            double real_delta = std::chrono::duration<double>(
                    simulation_now - simulation_previous
                ).count();
            simulation_previous = simulation_now;

            if (real_delta < 0.0)
            {
                real_delta = 0.0;
            }
            else if (real_delta > MAX_SIMULATION_DELTA_SECONDS)
            {
                real_delta = MAX_SIMULATION_DELTA_SECONDS;
            }

            simulation_accumulator += real_delta;

            while (simulation_accumulator >= SIMULATION_STEP_SECONDS)
            {
                Renderer::Test::updateScene(
                        &world,
                        &scene_state,
                        simulation_tick
                    );

                ++simulation_tick;
                simulation_accumulator -= SIMULATION_STEP_SECONDS;
            }
        }

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

        const auto frame_finished = Clock::now();

        if (performance_mode)
        {
            const double elapsed_ms =
                std::chrono::duration<double, std::milli>(
                        frame_finished - performance_start
                    ).count();

            if (elapsed_ms >= performance_warmup_ms
                    && (performance_duration_ms <= 0.0
                        || elapsed_ms <= performance_duration_ms))
            {
                cpu_frame_samples.push_back(
                        std::chrono::duration<double, std::milli>(
                                frame_finished - frame_started
                            ).count()
                    );
            }

            if (performance_duration_ms > 0.0
                    && elapsed_ms >= performance_duration_ms)
            {
                break;
            }
        }

        /* RendererCheck intentionally advances exactly once per captured
         * frame so all existing deterministic references remain unchanged. */
        if (renderercheck_mode)
        {
            Renderer::Test::updateScene(
                    &world,
                    &scene_state,
                    frame
                );
        }

        if (renderercheck_mode 
                && rendercheck_frame_is_last(frame)) 
        {
            break;
        }
        
        if (!renderercheck_mode && !performance_mode)
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

    /* Profiler shutdown resolves pending GPU timer queries and closes its
     * metrics stream before CPU samples append to the same file. */
    renderer.shutdown();

    if (performance_mode
            && !Renderer::PerformanceMetrics::appendSamples(
                    "cpu_frame_ms",
                    cpu_frame_samples
                ))
    {
        std::fprintf(
                stderr,
                "RendererCheck perf: failed to write cpu_frame_ms samples\n"
            );
        exit_code = 2;
    }

    Mouse.destroy();
    Keyboard.destroy();
    Display.destroy();
    return exit_code;
}