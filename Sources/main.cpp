#include <lwcgl/lwcgl.h>
#include <rendercheck/capture.h>

#include "ECS/ecs.hpp"
#include "RENDER/render.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 870

static inline int capture_frame(std::uint64_t frame) {
    if (!rendercheck_capture_due(frame)) return 0;

    const int width = Display.getWidth();
    const int height = Display.getHeight();
    if (width <= 0 || height <= 0) return -1;

    const std::size_t row_bytes = static_cast<std::size_t>(width) * 3u;
    std::vector<std::uint8_t> pixels(row_bytes * static_cast<std::size_t>(height));

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    for (int y = 0; y < height / 2; ++y) {
        std::uint8_t* top = pixels.data() + static_cast<std::size_t>(y) * row_bytes;
        std::uint8_t* bottom = pixels.data() + static_cast<std::size_t>(height - 1 - y) * row_bytes;

        for (std::size_t x = 0; x < row_bytes; ++x) {
            const std::uint8_t tmp = top[x];
            top[x] = bottom[x];
            bottom[x] = tmp;
        }
    }

    const int result = rendercheck_capture_rgb8(
        pixels.data(),
        static_cast<std::uint32_t>(width),
        static_cast<std::uint32_t>(height),
        row_bytes
    );

    return result < 0 ? -1 : 0;
}

static inline void ERROR(const char* operation) {
    const char* message = lwcglGetLastError();
    std::fprintf(stderr, "%s failed: %s\n", operation, message ? message : "unknown lwcgl error");
}

static ecs::Entity create_scene(ecs::World& world) {
    const ecs::Entity camera = world.createEntity();
    world.addTransform(camera, {
        {0.0f, 3.0f, 8.0f},
        {-12.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f},
    });
    world.addCamera(camera, {60.0f, 0.1f, 100.0f, true});

    const ecs::Entity cube = world.createEntity();
    world.addTransform(cube, {
        {0.0f, 1.45f, 0.0f},
        {-35.2643897f, 0.0f, 45.0f},
        {1.0f, 1.0f, 1.0f},
    });
    world.addRenderable(cube, {
        ecs::Primitive::Cube,
        {1.0f, 1.0f, 1.0f},
    });

    const ecs::Entity ground = world.createEntity();
    world.addTransform(ground, {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {14.0f, 1.0f, 14.0f},
    });
    world.addRenderable(ground, {
        ecs::Primitive::Plane,
        {0.28f, 0.30f, 0.34f},
    });

    return cube;
}

int main() {
    const bool renderercheck_mode = rendercheck_capture_requested() != 0;
    std::uint64_t frame = 0;
    int exit_code = 0;

    const DisplayMode mode = DisplayMode(WINDOW_WIDTH, WINDOW_HEIGHT);
    if (Display.setDisplayMode(&mode) != 0 || Display.create() != 0) {
        ERROR("Display.create");
        return 1;
    }

    if (Keyboard.create() != 0) {
        ERROR("Keyboard.create");
        Display.destroy();
        return 1;
    }

    if (Mouse.create() != 0) {
        ERROR("Mouse.create");
        Keyboard.destroy();
        Display.destroy();
        return 1;
    }

    ecs::World world;
    const ecs::Entity cube = create_scene(world);

    render::Renderer renderer;
    if (!renderer.init()) {
        std::fprintf(stderr, "Renderer initialization failed\n");
        Mouse.destroy();
        Keyboard.destroy();
        Display.destroy();
        return 1;
    }

    while (!Display.isCloseRequested() && !Keyboard.isKeyDown(Keyboard.KEY_ESCAPE)) {
        renderer.resize(Display.getWidth(), Display.getHeight());
        renderer.render(world);
        glFinish();

        if (capture_frame(frame) != 0) {
            std::fprintf(stderr, "RendererCheck framebuffer capture failed\n");
            exit_code = 2;
            break;
        }

        Display.update();

        if (ecs::TransformComponent* cube_transform = world.getTransform(cube)) {
            cube_transform->rotation.y += 0.65f;
            if (cube_transform->rotation.y >= 360.0f) cube_transform->rotation.y -= 360.0f;
        }

        if (renderercheck_mode && rendercheck_frame_is_last(frame)) break;
        if (!renderercheck_mode) Display.sync(60);
        ++frame;
    }

    renderer.shutdown();
    Mouse.destroy();
    Keyboard.destroy();
    Display.destroy();
    return exit_code;
}
