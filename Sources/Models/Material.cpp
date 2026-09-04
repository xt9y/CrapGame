#include "Models/Material.hpp"

#include "Models/Texture.hpp"
#include "Renderer/Material/Material.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <string>

namespace Models
{
namespace
{

float clamp01(float value)
{
    return std::max(0.0f, std::min(1.0f, value));
}

bool hasPath(const TextureRef& ref)
{
    return !ref.path.empty();
}

bool nearZero(float value)
{
    return std::fabs(value) <= 0.0001f;
}

bool black(const Renderer::Math::Vec3& value)
{
    return nearZero(value.x) && nearZero(value.y) && nearZero(value.z);
}

bool isTransmissionSemantic(const MaterialData& material)
{
    return material.transmission > 0.0001f
        || hasPath(material.transmission_texture);
}

Renderer::Material::ColorSpace colorSpaceFor(Renderer::Material::Slot slot)
{
    using Renderer::Material::ColorSpace;
    using Renderer::Material::Slot;
    switch (slot)
    {
        case Slot::BaseColor:
        case Slot::Ambient:
        case Slot::Specular:
        case Slot::Emissive:
        case Slot::Reflection:
        case Slot::Transmission:
            return ColorSpace::Srgb;
        default:
            return ColorSpace::Linear;
    }
}

Renderer::Material::TextureBinding bindingFor(
    const TextureRef& ref,
    Renderer::Material::Slot slot,
    std::vector<std::string> *warnings)
{
    Renderer::Material::TextureBinding binding;
    binding.offset = ref.offset;
    binding.scale = ref.scale;
    binding.turbulence = ref.turbulence;
    binding.multiplier = ref.bump_multiplier;
    binding.channel = ref.channel;
    binding.clamp = ref.clamp;
    binding.color_space = colorSpaceFor(slot);

    if (!ref.path.empty())
    {
        std::string error;
        binding.texture = loadTexture(ref.path, &error);
        if (binding.texture == INVALID_TEXTURE && warnings)
        {
            warnings->push_back(
                "material texture unavailable: " + ref.path
                + (error.empty() ? std::string{} : " (" + error + ")")
            );
        }
    }
    return binding;
}

float selectedChannel(const TextureAsset& asset, std::size_t pixel, char channel)
{
    const std::size_t base = pixel * 4u;
    const char key = static_cast<char>(std::tolower(static_cast<unsigned char>(channel)));
    std::size_t component = 0u;
    if (key == 'g') component = 1u;
    else if (key == 'b') component = 2u;
    else if (key == 'a') component = 3u;
    else if (key == 'm' || key == 'l')
    {
        const unsigned value = static_cast<unsigned>(asset.image.rgba[base])
            + static_cast<unsigned>(asset.image.rgba[base + 1u])
            + static_cast<unsigned>(asset.image.rgba[base + 2u]);
        return static_cast<float>(value) / (3.0f * 255.0f);
    }
    else if (key == '\0' && asset.image.meaningful_alpha)
    {
        component = 3u;
    }
    return static_cast<float>(asset.image.rgba[base + component]) / 255.0f;
}

bool opacityIsBinary(const Renderer::Material::TextureBinding& binding)
{
    const TextureAsset *asset = texture(binding.texture);
    if (!asset || asset->image.rgba.empty()) return true;
    const std::size_t pixels = asset->image.rgba.size() / 4u;
    for (std::size_t i = 0; i < pixels; ++i)
    {
        const float value = selectedChannel(*asset, i, binding.channel);
        if (value > 5.0f / 255.0f && value < 250.0f / 255.0f) return false;
    }
    return true;
}

void warnDerivedFailure(std::vector<std::string> *warnings,
                        const std::string& error)
{
    if (warnings && !error.empty())
        warnings->push_back("map_Ns roughness conversion failed: " + error);
}

} // namespace

bool detectLegacyZeroDIsOpaque(const std::vector<MaterialData>& materials)
{
    std::size_t ordinary_textured = 0u;
    std::size_t ordinary_zero = 0u;
    std::size_t alpha_materials = 0u;
    bool alpha_scalar_convention = true;
    bool competing_transmission = false;

    for (const MaterialData& material : materials)
    {
        const bool alpha_map = hasPath(material.opacity_texture);
        if (alpha_map)
        {
            ++alpha_materials;
            if (material.opacity < 0.999f) alpha_scalar_convention = false;
            continue;
        }
        if (isTransmissionSemantic(material)
                || (material.opacity > 0.0001f && material.opacity < 0.999f))
        {
            competing_transmission = true;
            continue;
        }
        if (!hasPath(material.base_color_texture)) continue;
        ++ordinary_textured;
        if (material.opacity <= 0.0001f) ++ordinary_zero;
    }

    return ordinary_textured > 0u
        && ordinary_zero * 100u >= ordinary_textured * 80u
        && alpha_materials > 0u
        && alpha_scalar_convention
        && !competing_transmission;
}

Renderer::Material::Resource resolveMaterial(
    const MaterialData& material,
    bool legacy_zero_d_is_opaque,
    std::vector<std::string> *warnings)
{
    using Renderer::Material::RenderClass;
    using Renderer::Material::Slot;

    Renderer::Material::Resource out;
    out.base_color = material.base_color;
    out.ambient = material.ambient;
    out.specular = material.specular;
    out.emissive = material.emissive;
    out.transmission_color = material.transmission_color;

    /* Texture-driven extension properties must remain active when their MTL
     * scalar is omitted. The parser's legacy scalar defaults are zero for
     * these extensions, so the presence of a map supplies the neutral base. */
    out.metallic = clamp01(
        hasPath(material.metallic_texture) && nearZero(material.metallic)
            ? 1.0f : material.metallic);
    out.shininess = std::max(0.0f, material.shininess);
    out.roughness = clamp01(material.roughness);
    if (!hasPath(material.roughness_texture)
            && !hasPath(material.shininess_texture)
            && out.shininess > 0.0f && material.roughness >= 0.999f)
    {
        out.roughness = Renderer::Material::nsToRoughness(out.shininess);
    }
    out.specular_strength = std::max(0.0f, material.specular_strength);
    out.ior = std::max(1.0001f, material.ior);
    out.transmission = clamp01(
        hasPath(material.transmission_texture) && nearZero(material.transmission)
            ? 1.0f : material.transmission);
    out.reflectivity = clamp01(
        hasPath(material.reflection_texture) && nearZero(material.reflectivity)
            ? 1.0f : material.reflectivity);
    out.clearcoat = clamp01(
        hasPath(material.clearcoat_texture) && nearZero(material.clearcoat)
            ? 1.0f : material.clearcoat);
    out.clearcoat_roughness = clamp01(
        hasPath(material.clearcoat_roughness_texture)
                && nearZero(material.clearcoat_roughness)
            ? 1.0f : material.clearcoat_roughness);
    out.sheen = clamp01(
        hasPath(material.sheen_texture) && nearZero(material.sheen)
            ? 1.0f : material.sheen);
    out.anisotropy = std::max(-0.95f, std::min(0.95f,
        hasPath(material.anisotropy_texture) && nearZero(material.anisotropy)
            ? 0.95f : material.anisotropy));
    out.illumination_model = material.illumination_model;

    if (hasPath(material.emissive_texture) && black(out.emissive))
        out.emissive = {1.0f, 1.0f, 1.0f};
    if (hasPath(material.specular_texture) && black(out.specular))
        out.specular = {1.0f, 1.0f, 1.0f};

    float opacity = clamp01(material.opacity);
    if (legacy_zero_d_is_opaque
            && opacity <= 0.0001f
            && !hasPath(material.opacity_texture)
            && !isTransmissionSemantic(material))
    {
        opacity = 1.0f;
    }
    out.opacity = opacity;

    const TextureRef *refs[] = {
        &material.base_color_texture,
        &material.ambient_texture,
        &material.specular_texture,
        &material.emissive_texture,
        &material.metallic_texture,
        &material.roughness_texture,
        &material.shininess_texture,
        &material.opacity_texture,
        &material.normal_texture,
        &material.bump_texture,
        &material.displacement_texture,
        &material.reflection_texture,
        &material.transmission_texture,
        &material.clearcoat_texture,
        &material.clearcoat_roughness_texture,
        &material.sheen_texture,
        &material.anisotropy_texture,
    };
    for (std::size_t i = 0; i < Renderer::Material::slotIndex(Slot::Count); ++i)
    {
        out.textures[i] = bindingFor(
            *refs[i], static_cast<Slot>(i), warnings);
    }

    /* map_Ns is intentionally converted into a cached linear roughness map.
     * That makes it visible in GBuffer, transparency and ray-material paths
     * without requiring a seventeenth live material sampler. */
    const std::size_t roughness_index = Renderer::Material::slotIndex(Slot::Roughness);
    const std::size_t shininess_index = Renderer::Material::slotIndex(Slot::Shininess);
    if (out.textures[roughness_index].texture == INVALID_TEXTURE
            && out.textures[shininess_index].texture != INVALID_TEXTURE)
    {
        Renderer::Material::TextureBinding derived = out.textures[shininess_index];
        std::string conversion_error;
        derived.texture = shininessToRoughnessTexture(
            derived.texture, derived.channel, &conversion_error);
        if (derived.texture != INVALID_TEXTURE)
        {
            derived.channel = 'r';
            derived.color_space = Renderer::Material::ColorSpace::Linear;
            out.textures[roughness_index] = derived;
            out.textures[shininess_index] = Renderer::Material::TextureBinding{};
            out.roughness = 1.0f;
        }
        else
        {
            warnDerivedFailure(warnings, conversion_error);
        }
    }

    if (!material.displacement_texture.path.empty())
    {
        std::string stem = std::filesystem::path(material.displacement_texture.path).stem().string();
        for (char& c : stem) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (stem.size() >= 4u && stem.compare(stem.size() - 4u, 4u, "_ddn") == 0)
        {
            const std::size_t normal = Renderer::Material::slotIndex(Slot::Normal);
            const std::size_t displacement = Renderer::Material::slotIndex(Slot::Displacement);
            if (out.textures[normal].texture == INVALID_TEXTURE)
                out.textures[normal] = out.textures[displacement];
            out.textures[displacement] = Renderer::Material::TextureBinding{};
        }
    }

    const bool transmission = out.transmission > 0.0001f
        || out.textures[Renderer::Material::slotIndex(Slot::Transmission)].texture != INVALID_TEXTURE;
    const bool opacity_map = hasPath(material.opacity_texture);

    if (transmission)
    {
        out.render_class = RenderClass::Transmissive;
    }
    else if (opacity_map)
    {
        const auto& binding = out.textures[Renderer::Material::slotIndex(Slot::Opacity)];
        out.render_class = opacityIsBinary(binding)
            ? RenderClass::Masked
            : RenderClass::Transparent;
    }
    else if (out.opacity < 0.999f)
    {
        out.render_class = RenderClass::Transparent;
    }
    else
    {
        out.render_class = RenderClass::Opaque;
    }

    return out;
}

} // namespace Models
