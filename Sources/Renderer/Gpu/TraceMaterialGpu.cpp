#include "Renderer/Gpu/TraceMaterialGpu.hpp"

#include "Models/Texture.hpp"
#include "Renderer/Gpu/MaterialGpu.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace Renderer { namespace Gpu { namespace {

constexpr int GUTTER = 4;

void setError(std::string *error, const char *message)
{
    if (error) *error = message ? message : "trace material GPU error";
}

int channelCode(char c)
{
    if (c == 'g' || c == 'G') return 1;
    if (c == 'b' || c == 'B') return 2;
    if (c == 'a' || c == 'A') return 3;
    if (c == 'm' || c == 'M' || c == 'l' || c == 'L') return 4;
    return 0;
}

void deleteTexture(GLuint *texture)
{
    if (texture && *texture != 0)
    {
        glDeleteTextures(*texture);
        *texture = 0;
    }
}

void deleteBuffer(GLuint *buffer)
{
    if (buffer && *buffer != 0)
    {
        GL15.glDeleteBuffers(1, buffer);
        *buffer = 0;
    }
}

std::vector<std::uint8_t> pageWithGutter(
    const Models::TextureAsset& asset, int page_width, int page_height)
{
    std::vector<std::uint8_t> page(
        static_cast<std::size_t>(page_width) *
        static_cast<std::size_t>(page_height) * 4u, 0u);
    const bool flip_normal_y = Models::normalMapUsesNegativeY(asset.path);
    for (int y = -GUTTER; y < asset.image.height + GUTTER; ++y)
    {
        const int sy = std::max(0, std::min(asset.image.height - 1, y));
        const int dy = y + GUTTER;
        if (dy < 0 || dy >= page_height) continue;
        for (int x = -GUTTER; x < asset.image.width + GUTTER; ++x)
        {
            const int sx = std::max(0, std::min(asset.image.width - 1, x));
            const int dx = x + GUTTER;
            if (dx < 0 || dx >= page_width) continue;
            const std::size_t source =
                (static_cast<std::size_t>(sy) * asset.image.width + sx) * 4u;
            const std::size_t destination =
                (static_cast<std::size_t>(dy) * page_width + dx) * 4u;
            std::memcpy(page.data() + destination,
                        asset.image.rgba.data() + source, 4u);
            if (flip_normal_y)
            {
                page[destination + 1u] = static_cast<std::uint8_t>(
                    255u - page[destination + 1u]);
            }
        }
    }
    return page;
}

} // namespace

bool TraceMaterialGpu::uploadBuffer(const void *data, std::size_t size,
                                    std::string *error)
{
    if (record_buffer_ == 0) GL15.glGenBuffers(1, &record_buffer_);
    if (record_buffer_ == 0)
    {
        setError(error, "failed to allocate trace record buffer");
        return false;
    }
    GL15.glBindBuffer(GL_SHADER_STORAGE_BUFFER, record_buffer_);
    const std::size_t bytes = std::max<std::size_t>(16u, size);
    GL15.glBufferData(GL_SHADER_STORAGE_BUFFER,
                      static_cast<LWCGLsizeiptr>(bytes),
                      size == 0 ? nullptr : data,
                      GL_STATIC_DRAW);
    return true;
}

int TraceMaterialGpu::packedLayer(
    Models::TextureHandle source, Material::ColorSpace color_space) const
{
    for (const PackedImage& image : packed_images_)
    {
        if (image.source == source && image.color_space == color_space)
            return image.layer;
    }
    return -1;
}

bool TraceMaterialGpu::rebuildAtlases(std::string *error)
{
    deleteTexture(&color_atlas_);
    deleteTexture(&data_atlas_);
    packed_images_.clear();

    int color_layers = 0;
    int data_layers = 0;
    int max_color_w = 1, max_color_h = 1;
    int max_data_w = 1, max_data_h = 1;

    for (std::size_t material_index = 0;
         material_index < Material::count(); ++material_index)
    {
        const Material::Resource *material = Material::get(
            static_cast<Material::MaterialHandle>(material_index));
        if (!material) continue;
        for (std::size_t slot_index = 0;
             slot_index < MaterialGpu::LIVE_TEXTURE_COUNT; ++slot_index)
        {
            const Material::Slot slot = MaterialGpu::liveSlot(slot_index);
            const Material::TextureBinding& binding =
                material->textures[Material::slotIndex(slot)];
            if (binding.texture == Models::INVALID_TEXTURE) continue;
            if (packedLayer(binding.texture, binding.color_space) >= 0) continue;
            const Models::TextureAsset *asset = Models::texture(binding.texture);
            if (!asset || asset->image.width <= 0 || asset->image.height <= 0
                    || asset->image.rgba.size() !=
                       static_cast<std::size_t>(asset->image.width) *
                       static_cast<std::size_t>(asset->image.height) * 4u)
            {
                continue;
            }
            const bool color = binding.color_space == Material::ColorSpace::Srgb;
            PackedImage packed;
            packed.source = binding.texture;
            packed.color_space = binding.color_space;
            packed.layer = color ? color_layers++ : data_layers++;
            packed.width = asset->image.width;
            packed.height = asset->image.height;
            packed_images_.push_back(packed);
            if (color)
            {
                max_color_w = std::max(max_color_w, asset->image.width);
                max_color_h = std::max(max_color_h, asset->image.height);
            }
            else
            {
                max_data_w = std::max(max_data_w, asset->image.width);
                max_data_h = std::max(max_data_h, asset->image.height);
            }
        }
    }

    color_page_width_ = max_color_w + GUTTER * 2;
    color_page_height_ = max_color_h + GUTTER * 2;
    data_page_width_ = max_data_w + GUTTER * 2;
    data_page_height_ = max_data_h + GUTTER * 2;

    auto allocate = [&](GLuint *texture, bool color, int width, int height,
                        int layers) -> bool
    {
        *texture = lwcgl_glGenTexture();
        if (*texture == 0) return false;
        glBindTexture(GL_TEXTURE_2D_ARRAY, *texture);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER,
                        GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        GLModern.glTexImage3D(
            GL_TEXTURE_2D_ARRAY, 0,
            color ? static_cast<GLint>(GL_SRGB8_ALPHA8)
                  : static_cast<GLint>(GL_RGBA8),
            width, height, std::max(1, layers), 0,
            GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        return true;
    };

    if (!allocate(&color_atlas_, true, color_page_width_, color_page_height_,
                  color_layers)
            || !allocate(&data_atlas_, false, data_page_width_, data_page_height_,
                         data_layers))
    {
        setError(error, "failed to allocate trace texture atlases");
        return false;
    }

    for (const PackedImage& packed : packed_images_)
    {
        const Models::TextureAsset *asset = Models::texture(packed.source);
        if (!asset) continue;
        const bool color = packed.color_space == Material::ColorSpace::Srgb;
        const int width = color ? color_page_width_ : data_page_width_;
        const int height = color ? color_page_height_ : data_page_height_;
        std::vector<std::uint8_t> page = pageWithGutter(*asset, width, height);
        glBindTexture(GL_TEXTURE_2D_ARRAY, color ? color_atlas_ : data_atlas_);
        GLModern.glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, packed.layer,
                                width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                                page.data());
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, color_atlas_);
    GL30.glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glBindTexture(GL_TEXTURE_2D_ARRAY, data_atlas_);
    GL30.glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    if (error) error->clear();
    return true;
}

bool TraceMaterialGpu::rebuild(std::string *error)
{
    const std::uint64_t revision = Material::revision();
    if (revision == material_revision_ && record_buffer_ != 0
            && color_atlas_ != 0 && data_atlas_ != 0)
    {
        if (error) error->clear();
        return true;
    }

    if (!rebuildAtlases(error)) return false;
    material_count_ = Material::count();
    records_.clear();
    records_.resize(material_count_);

    struct PendingDescriptor
    {
        std::size_t material = 0u;
        std::size_t live_slot = 0u;
        TraceMaterialRecordGpu record;
    };
    std::vector<PendingDescriptor> descriptors;

    for (std::size_t material_index = 0;
         material_index < material_count_; ++material_index)
    {
        const Material::Resource *material = Material::get(
            static_cast<Material::MaterialHandle>(material_index));
        if (!material) continue;
        TraceMaterialRecordGpu& out = records_[material_index];
        out.base_metallic[0] = material->base_color.x;
        out.base_metallic[1] = material->base_color.y;
        out.base_metallic[2] = material->base_color.z;
        out.base_metallic[3] = material->metallic;
        out.emissive_roughness[0] = material->emissive.x;
        out.emissive_roughness[1] = material->emissive.y;
        out.emissive_roughness[2] = material->emissive.z;
        out.emissive_roughness[3] = material->roughness;
        out.specular_ior[0] = material->specular.x * material->specular_strength;
        out.specular_ior[1] = material->specular.y * material->specular_strength;
        out.specular_ior[2] = material->specular.z * material->specular_strength;
        out.specular_ior[3] = material->ior;
        out.advanced[0] = material->opacity;
        out.advanced[1] = material->reflectivity;
        out.advanced[2] = material->clearcoat;
        out.advanced[3] = material->alpha_cutoff;
        out.transmission[0] = material->transmission_color.x;
        out.transmission[1] = material->transmission_color.y;
        out.transmission[2] = material->transmission_color.z;
        out.transmission[3] = material->transmission;
        out.extra[0] = material->clearcoat_roughness;
        out.extra[1] = material->sheen;
        out.extra[2] = material->anisotropy;
        out.extra[3] = static_cast<float>(material->render_class);
        std::fill(std::begin(out.textures), std::end(out.textures), -1);

        for (std::size_t live_index = 0;
             live_index < MaterialGpu::LIVE_TEXTURE_COUNT; ++live_index)
        {
            const Material::Slot slot = MaterialGpu::liveSlot(live_index);
            const Material::TextureBinding& binding =
                material->textures[Material::slotIndex(slot)];
            if (binding.texture == Models::INVALID_TEXTURE) continue;
            const Models::TextureAsset *asset = Models::texture(binding.texture);
            const int layer = packedLayer(binding.texture, binding.color_space);
            if (!asset || layer < 0) continue;
            const bool color = binding.color_space == Material::ColorSpace::Srgb;
            const int page_w = color ? color_page_width_ : data_page_width_;
            const int page_h = color ? color_page_height_ : data_page_height_;
            PendingDescriptor pending;
            pending.material = material_index;
            pending.live_slot = live_index;
            TraceMaterialRecordGpu& descriptor = pending.record;
            descriptor.base_metallic[0] = static_cast<float>(GUTTER) / page_w;
            descriptor.base_metallic[1] = static_cast<float>(GUTTER) / page_h;
            descriptor.base_metallic[2] = static_cast<float>(asset->image.width) / page_w;
            descriptor.base_metallic[3] = static_cast<float>(asset->image.height) / page_h;
            descriptor.emissive_roughness[0] = binding.scale.x;
            descriptor.emissive_roughness[1] = binding.scale.y;
            descriptor.emissive_roughness[2] = binding.offset.x + binding.turbulence.x;
            descriptor.emissive_roughness[3] = binding.offset.y + binding.turbulence.y;
            std::fill(std::begin(descriptor.textures), std::end(descriptor.textures), 0);
            descriptor.textures[0] = layer;
            descriptor.textures[1] = color ? 0 : 1;
            descriptor.textures[2] = (binding.channel == '\0'
                && slot == Material::Slot::Opacity && asset->image.meaningful_alpha)
                ? 3 : channelCode(binding.channel);
            descriptor.textures[3] = binding.clamp ? 1 : 0;
            descriptors.push_back(pending);
        }
    }

    records_.reserve(material_count_ + descriptors.size());
    for (PendingDescriptor& pending : descriptors)
    {
        const std::int32_t record_index =
            static_cast<std::int32_t>(records_.size());
        records_[pending.material].textures[pending.live_slot] = record_index;
        records_.push_back(pending.record);
    }

    if (!uploadBuffer(records_.data(),
                      records_.size() * sizeof(TraceMaterialRecordGpu), error))
    {
        return false;
    }
    material_revision_ = revision;
    if (error) error->clear();
    return true;
}

void TraceMaterialGpu::shutdown()
{
    deleteBuffer(&record_buffer_);
    deleteTexture(&color_atlas_);
    deleteTexture(&data_atlas_);
    records_.clear();
    packed_images_.clear();
    material_count_ = 0u;
    color_page_width_ = color_page_height_ = 1;
    data_page_width_ = data_page_height_ = 1;
    material_revision_ = 0u;
}

} }
