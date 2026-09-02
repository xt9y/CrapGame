#include <lwcgl/lwcgl.h>
#include <rendercheck/capture.h>

#include <cstdint>
#include <cstdio>
#include <vector>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 870

static inline int capture_frame (std::uint64_t frame) 
{
    if (!rendercheck_capture_due(frame)) return 0;

    const int width  = Display.getWidth();
    const int height = Display.getHeight();

    if (width <= 0 || height <= 0) return -1;

    const std::size_t row_bytes = static_cast<std::size_t>(width) * 3u;
    std::vector<std::uint8_t> pixels(row_bytes * static_cast<std::size_t>(height));

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    for (int y = 0; y < height / 2; ++y) 
    {
        std::uint8_t *top = 
            pixels.data() + static_cast<std::size_t>(y) * row_bytes;

        std::uint8_t *bottom = 
            pixels.data() + static_cast<std::size_t>(height - 1 - y) * row_bytes;

        for (std::size_t x = 0; x < row_bytes; ++x) 
        {
            const std::uint8_t tmp = top[x];
            top[x] = bottom[x];
            bottom[x] = tmp;
        }
    }

    const int result = 
        rendercheck_capture_rgb8(
            pixels.data(),
            static_cast<std::uint32_t>(width),
            static_cast<std::uint32_t>(height),
            row_bytes
        );

    return result < 0 ? -1 : 0;
}

static inline void ERROR (const char *operation) 
{
    const char *message = lwcglGetLastError();
    std::fprintf(
            stderr, "%s failed: %s\n", 
                operation, message ? 
                message : "unknown lwcgl error"
        );
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

    while (!Display.isCloseRequested() 
            && !Keyboard.isKeyDown(Keyboard.KEY_ESCAPE)) 
    {
        glViewport(0, 0, Display.getWidth(), Display.getHeight());
        glClearColor(0.055f, 0.070f, 0.105f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glFinish();

        if (capture_frame(frame) != 0) 
        {
            std::fprintf(stderr, "RendererCheck framebuffer capture failed\n");
            exit_code = 2; break;
        }

        Display.update();

        if (renderercheck_mode && rendercheck_frame_is_last(frame)) break;
        if (!renderercheck_mode) Display.sync(60);
        ++frame;
    }

    Mouse.destroy();
    Keyboard.destroy();
    Display.destroy();
    return exit_code;
}
