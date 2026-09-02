#ifndef CRAPGAME_RENDER_HPP
#define CRAPGAME_RENDER_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/GBuffer/GBuffer.hpp"
#include "Renderer/Math/Math.hpp"
#include "Renderer/Shadows/Shadows.hpp"

#include <lwcgl/lwcgl.h>
#include <rendercheck/capture.h>

#include <cstdint>
#include <vector>

namespace Renderer 
{

class Rendering 
{

public:
    bool init ();
    void resize (int width, int height);
    void render (const Ecs::World& world);
    void shutdown ();

private:
    void applyCamera (
                const Ecs::TransformComponent& transform,
                const Ecs::CameraComponent& camera
        );

    void renderGeometry (const Ecs::World& world);

    void composeLighting (
                const Ecs::World& world,
                const Math::Vec3& camera_position
        );

    void present ();

    GBuffer::Buffer gbuffer_;
    Shadows::Scene shadows_;

    Math::Mat4 view_       = Math::identity(),
               projection_ = Math::identity();

    std::vector<std::uint8_t> color_buffer_,
                              present_buffer_;

    int width_  = 1,
        height_ = 1;
};

} // namespace Renderer

static inline int captureFrame (std::uint64_t frame) 
{
    if (!rendercheck_capture_due(frame)) 
    {
        return 0;
    }

    const int width  = Display.getWidth(),
              height = Display.getHeight();

    if (width <= 0 
            || height <= 0) 
    {
        return -1;
    }

    const std::size_t row_bytes = 
        static_cast<std::size_t>(width) * 3u;

    std::vector<std::uint8_t> pixels(
            row_bytes * static_cast<std::size_t>(height)
        );

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(
            0, 0, width, height, 
            GL_RGB, GL_UNSIGNED_BYTE, pixels.data()
        );

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

#endif
