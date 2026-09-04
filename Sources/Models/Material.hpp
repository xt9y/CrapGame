#ifndef CRAPGAME_MODELS_MATERIAL_HPP
#define CRAPGAME_MODELS_MATERIAL_HPP

#include "Renderer/Math/Math.hpp"

#include <string>
#include <vector>

namespace Renderer { namespace Material { struct Resource; } }

namespace Models
{

struct TextureRef
{
    std::string path;
    Renderer::Math::Vec3 offset = {0.0f, 0.0f, 0.0f};
    Renderer::Math::Vec3 scale = {1.0f, 1.0f, 1.0f};
    Renderer::Math::Vec3 turbulence = {0.0f, 0.0f, 0.0f};
    float bump_multiplier = 1.0f;
    char channel = '\0';
    bool clamp = false;
};

struct MaterialData
{
    std::string name;

    Renderer::Math::Vec3 base_color = {1.0f, 1.0f, 1.0f};
    Renderer::Math::Vec3 ambient = {0.0f, 0.0f, 0.0f};
    Renderer::Math::Vec3 specular = {0.0f, 0.0f, 0.0f};
    Renderer::Math::Vec3 emissive = {0.0f, 0.0f, 0.0f};
    Renderer::Math::Vec3 transmission_color = {1.0f, 1.0f, 1.0f};

    float metallic = 0.0f;
    float roughness = 1.0f;
    float specular_strength = 1.0f;
    float shininess = 0.0f;
    float ior = 1.0f;
    float opacity = 1.0f;
    float transparency = 0.0f;
    float transmission = 0.0f;
    float reflectivity = 0.0f;
    float clearcoat = 0.0f;
    float clearcoat_roughness = 0.0f;
    float sheen = 0.0f;
    float anisotropy = 0.0f;
    int illumination_model = 0;

    TextureRef base_color_texture;
    TextureRef ambient_texture;
    TextureRef specular_texture;
    TextureRef emissive_texture;
    TextureRef metallic_texture;
    TextureRef roughness_texture;
    TextureRef shininess_texture;
    TextureRef opacity_texture;
    TextureRef normal_texture;
    TextureRef bump_texture;
    TextureRef displacement_texture;
    TextureRef reflection_texture;
    TextureRef transmission_texture;
    TextureRef clearcoat_texture;
    TextureRef clearcoat_roughness_texture;
    TextureRef sheen_texture;
    TextureRef anisotropy_texture;
};

bool detectLegacyZeroDIsOpaque(const std::vector<MaterialData>& materials);
Renderer::Material::Resource resolveMaterial(
    const MaterialData& material,
    bool legacy_zero_d_is_opaque,
    std::vector<std::string> *warnings = nullptr
);

} // namespace Models

#endif
