#ifndef CRAPGAME_RENDERER_MATERIAL_HPP
#define CRAPGAME_RENDERER_MATERIAL_HPP

#include "Models/Texture.hpp"
#include "Renderer/Math/Math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace Renderer
{
namespace Material
{

using MaterialHandle = std::uint32_t;
constexpr MaterialHandle INVALID_MATERIAL = UINT32_MAX;

enum class RenderClass : std::uint8_t
{
    Opaque,
    Masked,
    Transparent,
    Transmissive,
};

enum class ColorSpace : std::uint8_t
{
    Linear,
    Srgb,
};

enum class Slot : std::uint8_t
{
    BaseColor,
    Ambient,
    Specular,
    Emissive,
    Metallic,
    Roughness,
    Shininess,
    Opacity,
    Normal,
    Bump,
    Displacement,
    Reflection,
    Transmission,
    Clearcoat,
    ClearcoatRoughness,
    Sheen,
    Anisotropy,
    Count,
};

constexpr std::size_t slotIndex(Slot slot)
{
    return static_cast<std::size_t>(slot);
}

struct TextureBinding
{
    Models::TextureHandle texture = Models::INVALID_TEXTURE;
    Math::Vec3 offset = {0.0f, 0.0f, 0.0f};
    Math::Vec3 scale = {1.0f, 1.0f, 1.0f};
    Math::Vec3 turbulence = {0.0f, 0.0f, 0.0f};
    float multiplier = 1.0f;
    char channel = '\0';
    bool clamp = false;
    ColorSpace color_space = ColorSpace::Linear;
};

struct Resource
{
    Math::Vec3 base_color = {1.0f, 1.0f, 1.0f};
    Math::Vec3 ambient = {0.0f, 0.0f, 0.0f};
    Math::Vec3 specular = {0.0f, 0.0f, 0.0f};
    Math::Vec3 emissive = {0.0f, 0.0f, 0.0f};
    Math::Vec3 transmission_color = {1.0f, 1.0f, 1.0f};

    float metallic = 0.0f;
    float roughness = 1.0f;
    float specular_strength = 1.0f;
    float shininess = 0.0f;
    float ior = 1.0f;
    float opacity = 1.0f;
    float transmission = 0.0f;
    float reflectivity = 0.0f;
    float clearcoat = 0.0f;
    float clearcoat_roughness = 0.0f;
    float sheen = 0.0f;
    float anisotropy = 0.0f;
    int illumination_model = 0;

    RenderClass render_class = RenderClass::Opaque;
    float alpha_cutoff = 0.5f;

    std::array<TextureBinding, slotIndex(Slot::Count)> textures = {};
};

float nsToRoughness(float shininess);
float iorToF0(float ior);

MaterialHandle registerMaterial(Resource resource);
const Resource *get(MaterialHandle handle);
std::size_t count();
std::uint64_t revision();
void clear();

} // namespace Material
} // namespace Renderer

#endif
