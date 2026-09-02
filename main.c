#include <lwcgl/lwcgl.h>
#include <rendercheck/capture.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 360

static void render_frame(void) {
    glViewport(0, 0, Display.getWidth(), Display.getHeight());
    glClearColor(0.055f, 0.070f, 0.105f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 1.0, 0.0, 1.0, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glBegin(GL_QUADS);
    glColor3f(0.20f, 0.58f, 0.95f);
    glVertex2f(0.10f, 0.18f);
    glVertex2f(0.44f, 0.18f);
    glVertex2f(0.44f, 0.72f);
    glVertex2f(0.10f, 0.72f);

    glColor3f(0.95f, 0.34f, 0.22f);
    glVertex2f(0.56f, 0.28f);
    glVertex2f(0.90f, 0.28f);
    glVertex2f(0.90f, 0.82f);
    glVertex2f(0.56f, 0.82f);
    glEnd();

    glBegin(GL_TRIANGLES);
    glColor3f(0.95f, 0.82f, 0.28f);
    glVertex2f(0.50f, 0.12f);
    glVertex2f(0.68f, 0.52f);
    glVertex2f(0.32f, 0.52f);
    glEnd();
}

static int capture_frame(uint64_t frame) {
    int width;
    int height;
    size_t row_bytes;
    uint8_t *pixels;
    int result;

    if (!rendercheck_capture_due(frame)) return 0;

    width = Display.getWidth();
    height = Display.getHeight();
    if (width <= 0 || height <= 0) return -1;

    row_bytes = (size_t)width * 3u;
    pixels = (uint8_t *)malloc(row_bytes * (size_t)height);
    if (!pixels) return -1;

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    for (int y = 0; y < height / 2; ++y) {
        uint8_t *top = pixels + (size_t)y * row_bytes;
        uint8_t *bottom = pixels + (size_t)(height - 1 - y) * row_bytes;
        for (size_t x = 0; x < row_bytes; ++x) {
            uint8_t tmp = top[x];
            top[x] = bottom[x];
            bottom[x] = tmp;
        }
    }

    result = rendercheck_capture_rgb8(
        pixels,
        (uint32_t)width,
        (uint32_t)height,
        row_bytes
    );
    free(pixels);
    return result < 0 ? -1 : 0;
}

static void print_lwcgl_error(const char *operation) {
    const char *message = lwcglGetLastError();
    fprintf(stderr, "%s failed: %s\n", operation, message ? message : "unknown lwcgl error");
}

int main(void) {
    DisplayMode mode = DisplayMode(WINDOW_WIDTH, WINDOW_HEIGHT);
    const int renderercheck_mode = rendercheck_capture_requested();
    uint64_t frame = 0;
    int exit_code = 0;

    Display.setTitle("CrapGame - lwcgl v2.9.3 - C");
    Display.setResizable(LWCGL_FALSE);
    Display.setVSyncEnabled(LWCGL_FALSE);

    if (Display.setDisplayMode(&mode) != 0) {
        print_lwcgl_error("Display.setDisplayMode");
        return 1;
    }
    if (Display.create() != 0) {
        print_lwcgl_error("Display.create");
        return 1;
    }
    if (Keyboard.create() != 0) {
        print_lwcgl_error("Keyboard.create");
        Display.destroy();
        return 1;
    }
    if (Mouse.create() != 0) {
        print_lwcgl_error("Mouse.create");
        Keyboard.destroy();
        Display.destroy();
        return 1;
    }

    while (!Display.isCloseRequested() && !Keyboard.isKeyDown(Keyboard.KEY_ESCAPE)) {
        render_frame();
        glFinish();

        if (capture_frame(frame) != 0) {
            fprintf(stderr, "RendererCheck framebuffer capture failed\n");
            exit_code = 2;
            break;
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
