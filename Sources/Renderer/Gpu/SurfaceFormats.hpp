#ifndef CRAPGAME_RENDERER_GPU_SURFACEFORMATS_HPP
#define CRAPGAME_RENDERER_GPU_SURFACEFORMATS_HPP

#include <cstddef>

namespace Renderer
{
namespace Gpu
{

enum class SurfaceFormat
{
    Rgba8,
    Rgba16f,
};

constexpr std::size_t bytesPerPixel (SurfaceFormat format)
{
    return format == SurfaceFormat::Rgba8 ? 4u : 8u;
}

constexpr SurfaceFormat GBUFFER_POSITION_DEPTH_FORMAT = SurfaceFormat::Rgba16f;
constexpr SurfaceFormat GBUFFER_NORMAL_ROUGHNESS_FORMAT = SurfaceFormat::Rgba16f;
constexpr SurfaceFormat GBUFFER_ALBEDO_METALLIC_FORMAT = SurfaceFormat::Rgba8;
constexpr SurfaceFormat GBUFFER_EMISSIVE_FORMAT = SurfaceFormat::Rgba16f;
constexpr SurfaceFormat DIRECT_COLOR_FORMAT = SurfaceFormat::Rgba16f;
constexpr SurfaceFormat LUMEN_HISTORY_FORMAT = SurfaceFormat::Rgba16f;
constexpr SurfaceFormat LUMEN_POSITION_HISTORY_FORMAT = SurfaceFormat::Rgba16f;
constexpr SurfaceFormat LUMEN_FINAL_FORMAT = SurfaceFormat::Rgba8;

constexpr std::size_t stage11SavedBytes (
            std::size_t width,
            std::size_t height
    )
{
    constexpr std::size_t SAVED_PER_PIXEL =
        (bytesPerPixel(SurfaceFormat::Rgba16f) - bytesPerPixel(GBUFFER_ALBEDO_METALLIC_FORMAT))
        + (bytesPerPixel(SurfaceFormat::Rgba16f) - bytesPerPixel(LUMEN_FINAL_FORMAT));

    return width * height * SAVED_PER_PIXEL;
}

} // namespace Gpu
} // namespace Renderer

#endif
