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

constexpr SurfaceFormat GBUFFER_NORMAL_ROUGHNESS_FORMAT = SurfaceFormat::Rgba16f;
constexpr SurfaceFormat GBUFFER_ALBEDO_METALLIC_FORMAT = SurfaceFormat::Rgba8;
constexpr SurfaceFormat GBUFFER_EMISSIVE_FORMAT = SurfaceFormat::Rgba16f;
constexpr SurfaceFormat GBUFFER_SPECULAR_IOR_FORMAT = SurfaceFormat::Rgba16f;
constexpr SurfaceFormat GBUFFER_ADVANCED_FORMAT = SurfaceFormat::Rgba16f;
constexpr SurfaceFormat GBUFFER_TRANSMISSION_FORMAT = SurfaceFormat::Rgba16f;
constexpr SurfaceFormat GBUFFER_TANGENT_ANISOTROPY_FORMAT = SurfaceFormat::Rgba16f;
constexpr SurfaceFormat DIRECT_COLOR_FORMAT = SurfaceFormat::Rgba16f;
constexpr SurfaceFormat LUMEN_HISTORY_FORMAT = SurfaceFormat::Rgba16f;
constexpr SurfaceFormat LUMEN_POSITION_HISTORY_FORMAT = SurfaceFormat::Rgba16f;
constexpr SurfaceFormat LUMEN_FINAL_FORMAT = SurfaceFormat::Rgba8;

constexpr std::size_t GBUFFER_MATERIAL_ID_BYTES_PER_PIXEL = 4u;
constexpr std::size_t GBUFFER_DEPTH_BYTES_PER_PIXEL = 4u;
constexpr std::size_t GBUFFER_LEGACY_POSITION_BYTES_PER_PIXEL = 8u;
constexpr std::size_t GBUFFER_STAGE_A_COLOR_BYTES_PER_PIXEL =
    bytesPerPixel(GBUFFER_NORMAL_ROUGHNESS_FORMAT)
    + bytesPerPixel(GBUFFER_ALBEDO_METALLIC_FORMAT)
    + bytesPerPixel(GBUFFER_EMISSIVE_FORMAT)
    + bytesPerPixel(GBUFFER_SPECULAR_IOR_FORMAT)
    + bytesPerPixel(GBUFFER_ADVANCED_FORMAT)
    + bytesPerPixel(GBUFFER_TRANSMISSION_FORMAT)
    + bytesPerPixel(GBUFFER_TANGENT_ANISOTROPY_FORMAT);
constexpr std::size_t GBUFFER_STAGE_A_BYTES_PER_PIXEL =
    GBUFFER_STAGE_A_COLOR_BYTES_PER_PIXEL
    + GBUFFER_MATERIAL_ID_BYTES_PER_PIXEL
    + GBUFFER_DEPTH_BYTES_PER_PIXEL;
constexpr std::size_t GBUFFER_LEGACY_BYTES_PER_PIXEL =
    GBUFFER_STAGE_A_BYTES_PER_PIXEL + GBUFFER_LEGACY_POSITION_BYTES_PER_PIXEL;
constexpr std::size_t GBUFFER_STAGE_A_SAVED_BYTES_PER_PIXEL =
    GBUFFER_LEGACY_BYTES_PER_PIXEL - GBUFFER_STAGE_A_BYTES_PER_PIXEL;

constexpr std::size_t gbufferStageASavedBytes(std::size_t width,std::size_t height)
{
    return width*height*GBUFFER_STAGE_A_SAVED_BYTES_PER_PIXEL;
}

constexpr std::size_t stage11SavedBytes(std::size_t width,std::size_t height)
{
    return gbufferStageASavedBytes(width,height);
}

} // namespace Gpu
} // namespace Renderer

#endif
