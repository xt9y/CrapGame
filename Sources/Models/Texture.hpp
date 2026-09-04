#ifndef CRAPGAME_MODELS_TEXTURE_HPP
#define CRAPGAME_MODELS_TEXTURE_HPP

#include "Models/Tga.hpp"

#include <cstdint>
#include <string>

namespace Models
{

using TextureHandle = std::uint32_t;
constexpr TextureHandle INVALID_TEXTURE = UINT32_MAX;

struct TextureAsset
{
    std::string path;
    Tga::Image image;
};

TextureHandle loadTexture(
    const std::string& path,
    std::string *error = nullptr
);

/* Crytek-era _ddn/_ddna normal maps use the Y-negative tangent convention.
 * OpenGL tangent space is Y-positive, so GPU consumers must invert green for
 * these assets instead of applying the inversion to every normal map. */
bool normalMapUsesNegativeY(const std::string& path);

/* map_Ns is intentionally preprocessed rather than consuming another live
 * sampler. Normalized map values are interpreted over the legacy MTL Ns
 * interval [0,1000], converted with sqrt(2/(Ns+2)), and cached as RGBA8. */
TextureHandle shininessToRoughnessTexture(
    TextureHandle shininess,
    char channel = '\0',
    std::string *error = nullptr
);

const TextureAsset *texture(TextureHandle handle);
void clearTextureCache();

} // namespace Models

#endif
