#include "Renderer/Gpu/SurfaceFormats.hpp"

#include <cstddef>
#include <cstdio>

int main ()
{
    using Renderer::Gpu::SurfaceFormat;
    using Renderer::Gpu::bytesPerPixel;

    static_assert(Renderer::Gpu::GBUFFER_POSITION_DEPTH_FORMAT == SurfaceFormat::Rgba16f);
    static_assert(Renderer::Gpu::GBUFFER_NORMAL_ROUGHNESS_FORMAT == SurfaceFormat::Rgba16f);
    static_assert(Renderer::Gpu::GBUFFER_ALBEDO_METALLIC_FORMAT == SurfaceFormat::Rgba8);
    static_assert(Renderer::Gpu::GBUFFER_EMISSIVE_FORMAT == SurfaceFormat::Rgba16f);
    static_assert(Renderer::Gpu::DIRECT_COLOR_FORMAT == SurfaceFormat::Rgba16f);
    static_assert(Renderer::Gpu::LUMEN_HISTORY_FORMAT == SurfaceFormat::Rgba16f);
    static_assert(Renderer::Gpu::LUMEN_POSITION_HISTORY_FORMAT == SurfaceFormat::Rgba16f);
    static_assert(Renderer::Gpu::LUMEN_FINAL_FORMAT == SurfaceFormat::Rgba8);

    static_assert(bytesPerPixel(SurfaceFormat::Rgba8) == 4u);
    static_assert(bytesPerPixel(SurfaceFormat::Rgba16f) == 8u);

    constexpr std::size_t width = 1280u;
    constexpr std::size_t height = 870u;
    constexpr std::size_t expected_saved_bytes = 8908800u;

    static_assert(
            Renderer::Gpu::stage11SavedBytes(width, height)
                == expected_saved_bytes
        );

    std::printf(
            "GPU surface format contract: %zu bytes saved at %zux%zu\n",
            Renderer::Gpu::stage11SavedBytes(width, height),
            width,
            height
        );
    return 0;
}
