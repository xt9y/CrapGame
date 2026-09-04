#include "Renderer/Gpu/MaterialGpu.hpp"

#include "Models/Texture.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Renderer
{
namespace Gpu
{
namespace
{

bool isNormalSlot(Material::Slot slot)
{
    return slot == Material::Slot::Normal;
}

struct MipLevel
{
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba;
};

std::uint8_t encodeNormal(float value)
{
    const float encoded = value * 0.5f + 0.5f;
    const float clamped = std::max(0.0f, std::min(1.0f, encoded));
    return static_cast<std::uint8_t>(std::lround(clamped * 255.0f));
}

std::vector<MipLevel> normalMipChain(const Models::Tga::Image& image)
{
    std::vector<MipLevel> levels;
    levels.push_back({image.width, image.height, image.rgba});

    while (levels.back().width > 1 || levels.back().height > 1)
    {
        const MipLevel& src = levels.back();
        MipLevel dst;
        dst.width = std::max(1, src.width / 2);
        dst.height = std::max(1, src.height / 2);
        dst.rgba.resize(static_cast<std::size_t>(dst.width * dst.height) * 4u);

        for (int y = 0; y < dst.height; ++y)
        {
            for (int x = 0; x < dst.width; ++x)
            {
                float nx = 0.0f, ny = 0.0f, nz = 0.0f, alpha = 0.0f;
                int samples = 0;
                for (int oy = 0; oy < 2; ++oy)
                {
                    for (int ox = 0; ox < 2; ++ox)
                    {
                        const int sx = std::min(src.width - 1, x * 2 + ox);
                        const int sy = std::min(src.height - 1, y * 2 + oy);
                        const std::size_t si =
                            (static_cast<std::size_t>(sy) * src.width + sx) * 4u;
                        nx += static_cast<float>(src.rgba[si]) / 127.5f - 1.0f;
                        ny += static_cast<float>(src.rgba[si + 1u]) / 127.5f - 1.0f;
                        nz += static_cast<float>(src.rgba[si + 2u]) / 127.5f - 1.0f;
                        alpha += static_cast<float>(src.rgba[si + 3u]);
                        ++samples;
                    }
                }
                const float length = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (length > 1.0e-8f)
                {
                    nx /= length;
                    ny /= length;
                    nz /= length;
                }
                else
                {
                    nx = 0.0f;
                    ny = 0.0f;
                    nz = 1.0f;
                }
                const std::size_t di =
                    (static_cast<std::size_t>(y) * dst.width + x) * 4u;
                dst.rgba[di] = encodeNormal(nx);
                dst.rgba[di + 1u] = encodeNormal(ny);
                dst.rgba[di + 2u] = encodeNormal(nz);
                dst.rgba[di + 3u] = static_cast<std::uint8_t>(
                    std::lround(alpha / static_cast<float>(samples))
                );
            }
        }
        levels.push_back(std::move(dst));
    }
    return levels;
}

void setError(std::string *error, const char *message)
{
    if (error) *error = message ? message : "GPU material error";
}

} // namespace

Material::Slot MaterialGpu::liveSlot(std::size_t index)
{
    static constexpr Material::Slot slots[LIVE_TEXTURE_COUNT] = {
        Material::Slot::BaseColor,
        Material::Slot::Ambient,
        Material::Slot::Specular,
        Material::Slot::Emissive,
        Material::Slot::Metallic,
        Material::Slot::Roughness,
        Material::Slot::Opacity,
        Material::Slot::Normal,
        Material::Slot::Bump,
        Material::Slot::Reflection,
        Material::Slot::Transmission,
        Material::Slot::Clearcoat,
        Material::Slot::ClearcoatRoughness,
        Material::Slot::Sheen,
        Material::Slot::Anisotropy,
    };
    return index < LIVE_TEXTURE_COUNT ? slots[index] : Material::Slot::Count;
}

bool MaterialGpu::init(std::string *error)
{
    shutdown();
    initialized_ = true;
    if (error) error->clear();
    return true;
}

GLuint MaterialGpu::findTexture(const Material::TextureBinding& binding,
                                Material::Slot slot) const
{
    if (binding.texture == Models::INVALID_TEXTURE) return 0;
    const bool normal = isNormalSlot(slot);
    for (const TextureGpu& gpu : textures_)
    {
        if (gpu.source == binding.texture
                && gpu.color_space == binding.color_space
                && gpu.normal_map == normal
                && gpu.clamp == binding.clamp)
        {
            return gpu.texture;
        }
    }
    return 0;
}

GLuint MaterialGpu::ensureTexture(const Material::TextureBinding& binding,
                                  Material::Slot slot,
                                  std::string *error)
{
    if (binding.texture == Models::INVALID_TEXTURE) return 0;
    if (const GLuint existing = findTexture(binding, slot)) return existing;

    const Models::TextureAsset *asset = Models::texture(binding.texture);
    if (!asset || asset->image.width <= 0 || asset->image.height <= 0
            || asset->image.rgba.size()
                != static_cast<std::size_t>(asset->image.width * asset->image.height) * 4u)
    {
        setError(error, "invalid CPU texture asset for GPU upload");
        return 0;
    }

    const GLuint texture = lwcgl_glGenTexture();
    if (texture == 0)
    {
        setError(error, "failed to allocate GPU material texture");
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
        binding.clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
        binding.clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);

    const GLint internal_format = binding.color_space == Material::ColorSpace::Srgb
        ? static_cast<GLint>(GL_SRGB8_ALPHA8)
        : static_cast<GLint>(GL_RGBA8);

    if (isNormalSlot(slot))
    {
        const std::vector<MipLevel> levels = normalMipChain(asset->image);
        for (std::size_t level = 0; level < levels.size(); ++level)
        {
            const MipLevel& mip = levels[level];
            glTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(level), internal_format,
                mip.width, mip.height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                mip.rgba.data());
        }
    }
    else
    {
        glTexImage2D(GL_TEXTURE_2D, 0, internal_format,
            asset->image.width, asset->image.height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, asset->image.rgba.data());
        GL30.glGenerateMipmap(GL_TEXTURE_2D);
    }

    textures_.push_back({binding.texture, binding.color_space,
                         isNormalSlot(slot), binding.clamp, texture});
    return texture;
}

bool MaterialGpu::ensure(Material::MaterialHandle material, std::string *error)
{
    if (!initialized_)
    {
        setError(error, "GPU material cache is not initialized");
        return false;
    }
    const Material::Resource *resource = Material::get(material);
    if (!resource)
    {
        setError(error, "invalid renderer material handle");
        return false;
    }
    for (std::size_t i = 0; i < Material::slotIndex(Material::Slot::Count); ++i)
    {
        const Material::Slot slot = static_cast<Material::Slot>(i);
        const Material::TextureBinding& binding = resource->textures[i];
        if (binding.texture != Models::INVALID_TEXTURE
                && ensureTexture(binding, slot, error) == 0)
        {
            return false;
        }
    }
    if (error) error->clear();
    return true;
}

std::uint32_t MaterialGpu::textureMask(Material::MaterialHandle material) const
{
    const Material::Resource *resource = Material::get(material);
    if (!resource) return 0u;
    std::uint32_t mask = 0u;
    for (std::size_t i = 0; i < Material::slotIndex(Material::Slot::Count); ++i)
    {
        if (resource->textures[i].texture != Models::INVALID_TEXTURE)
            mask |= (1u << static_cast<unsigned>(i));
    }
    return mask;
}

bool MaterialGpu::bind(Material::MaterialHandle material, GLuint first_unit,
                       std::string *error)
{
    if (!ensure(material, error)) return false;
    const Material::Resource *resource = Material::get(material);
    for (std::size_t i = 0; i < LIVE_TEXTURE_COUNT; ++i)
    {
        const Material::Slot slot = liveSlot(i);
        const Material::TextureBinding& binding =
            resource->textures[Material::slotIndex(slot)];
        const GLuint texture = findTexture(binding, slot);
        GLModern.glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + first_unit + i));
        glBindTexture(GL_TEXTURE_2D, texture);
    }
    GLModern.glActiveTexture(GL_TEXTURE0);
    if (error) error->clear();
    return true;
}

void MaterialGpu::shutdown()
{
    for (TextureGpu& gpu : textures_)
    {
        if (gpu.texture != 0) glDeleteTextures(gpu.texture);
        gpu.texture = 0;
    }
    textures_.clear();
    initialized_ = false;
}

} // namespace Gpu
} // namespace Renderer
