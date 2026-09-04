#include "Models/Material.hpp"
#include "Models/Texture.hpp"
#include "Renderer/Material/Material.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace
{

bool near(float a, float b)
{
    return std::fabs(a - b) < 0.001f;
}

void writeGrayTga(const std::filesystem::path& path, const std::vector<std::uint8_t>& values)
{
    std::vector<std::uint8_t> bytes(18u, 0u);
    bytes[2] = 3u;
    bytes[12] = static_cast<std::uint8_t>(values.size());
    bytes[14] = 1u;
    bytes[16] = 8u;
    bytes[17] = 0x20u;
    bytes.insert(bytes.end(), values.begin(), values.end());
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

Models::MaterialData ordinary(const std::filesystem::path& texture, float opacity)
{
    Models::MaterialData m;
    m.opacity = opacity;
    m.transparency = 1.0f - opacity;
    m.base_color_texture.path = texture.string();
    return m;
}

} // namespace

int main()
{
    namespace RM = Renderer::Material;
    assert(near(RM::nsToRoughness(18.0f), std::sqrt(2.0f / 20.0f)));
    assert(near(RM::iorToF0(1.5f), 0.04f));

    const std::filesystem::path dir = "/tmp/crapgame-material-resolver";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    writeGrayTga(dir / "binary.tga", {0u, 255u, 4u, 251u});
    writeGrayTga(dir / "fractional.tga", {0u, 128u, 255u});
    writeGrayTga(dir / "diffuse.tga", {128u});

    Models::clearTextureCache();
    std::vector<Models::MaterialData> doc;
    for (int i = 0; i < 8; ++i)
    {
        doc.push_back(ordinary(dir / "diffuse.tga", 0.0f));
    }
    doc.push_back(ordinary(dir / "diffuse.tga", 1.0f));
    doc.push_back(ordinary(dir / "diffuse.tga", 0.0f));
    Models::MaterialData alpha;
    alpha.opacity = 1.0f;
    alpha.opacity_texture.path = (dir / "binary.tga").string();
    doc.push_back(alpha);
    assert(Models::detectLegacyZeroDIsOpaque(doc));

    auto resolved = Models::resolveMaterial(doc[0], true);
    assert(resolved.render_class == RM::RenderClass::Opaque);
    assert(near(resolved.opacity, 1.0f));

    Models::MaterialData masked;
    masked.opacity = 1.0f;
    masked.opacity_texture.path = (dir / "binary.tga").string();
    masked.opacity_texture.channel = 'r';
    resolved = Models::resolveMaterial(masked, false);
    assert(resolved.render_class == RM::RenderClass::Masked);
    assert(resolved.textures[RM::slotIndex(RM::Slot::Opacity)].texture != Models::INVALID_TEXTURE);

    Models::MaterialData transparent = masked;
    transparent.opacity_texture.path = (dir / "fractional.tga").string();
    resolved = Models::resolveMaterial(transparent, false);
    assert(resolved.render_class == RM::RenderClass::Transparent);

    Models::MaterialData glass;
    glass.ior = 1.5f;
    glass.transmission = 0.8f;
    glass.transmission_color = {0.8f, 0.9f, 1.0f};
    resolved = Models::resolveMaterial(glass, false);
    assert(resolved.render_class == RM::RenderClass::Transmissive);
    assert(near(resolved.ior, 1.5f));

    Models::MaterialData full;
    full.base_color = {0.2f, 0.3f, 0.4f};
    full.metallic = 0.7f;
    full.roughness = 0.33f;
    full.specular = {0.5f, 0.4f, 0.3f};
    full.specular_strength = 0.8f;
    full.reflectivity = 0.6f;
    full.clearcoat = 0.5f;
    full.clearcoat_roughness = 0.12f;
    full.sheen = 0.25f;
    full.anisotropy = -0.4f;
    full.base_color_texture.path = (dir / "diffuse.tga").string();
    full.base_color_texture.offset = {1.0f, 2.0f, 3.0f};
    full.base_color_texture.scale = {2.0f, 2.0f, 1.0f};
    full.base_color_texture.clamp = true;
    resolved = Models::resolveMaterial(full, false);
    assert(near(resolved.metallic, 0.7f) && near(resolved.roughness, 0.33f));
    assert(near(resolved.reflectivity, 0.6f) && near(resolved.clearcoat, 0.5f));
    const auto& base = resolved.textures[RM::slotIndex(RM::Slot::BaseColor)];
    assert(base.texture != Models::INVALID_TEXTURE);
    assert(base.color_space == RM::ColorSpace::Srgb);
    assert(base.clamp && near(base.offset.x, 1.0f) && near(base.scale.x, 2.0f));

    RM::clear();
    const RM::MaterialHandle handle = RM::registerMaterial(resolved);
    assert(handle != RM::INVALID_MATERIAL && RM::get(handle));
    assert(RM::count() == 1u);
    RM::clear();
    assert(RM::count() == 0u);

    return 0;
}
