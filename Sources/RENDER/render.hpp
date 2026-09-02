#ifndef CRAPGAME_RENDER_HPP
#define CRAPGAME_RENDER_HPP

#include "ECS/ecs.hpp"

namespace render {

class Renderer {
public:
    bool init ();
    void resize (int width, int height);
    void render (const ecs::World& world);
    void shutdown ();

private:
    void applyCamera (const ecs::TransformComponent& transform, const ecs::CameraComponent& camera) const;
    void drawRenderable (const ecs::TransformComponent& transform, const ecs::RenderableComponent& renderable) const;
    void drawCube () const;
    void drawPlane() const;

    int width_ = 1;
    int height_ = 1;
};

} // namespace render

#include <rendercheck/capture.h>
#include <lwcgl/lwcgl.h>

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
        std::uint8_t* top = 
            pixels.data() + static_cast<std::size_t>(y) * row_bytes;
        std::uint8_t* bottom = 
            pixels.data() + static_cast<std::size_t>(height - 1 - y) * row_bytes;

        for (std::size_t x = 0; x < row_bytes; ++x) 
        {
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

#endif
