#ifndef CRAPGAME_RENDERER_GPU_TRACEMATERIALGPU_HPP
#define CRAPGAME_RENDERER_GPU_TRACEMATERIALGPU_HPP

#include "Renderer/Material/Material.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Renderer { namespace Gpu {

/* One std430 record type is deliberately used for both material records and
 * texture descriptors. This keeps the trace path inside the GL4.3 minimum of
 * eight shader-storage-buffer bindings while still supporting arbitrary
 * material texture lookups. Material records occupy [0, materialCount); their
 * texture indices address descriptor records appended after that range. */
struct TraceMaterialRecordGpu
{
    float base_metallic[4] = {};
    float emissive_roughness[4] = {};
    float specular_ior[4] = {};
    float advanced[4] = {};
    float transmission[4] = {};
    float extra[4] = {};
    std::int32_t textures[16] = {};
};
static_assert(sizeof(TraceMaterialRecordGpu) == 160u,
              "TraceMaterialRecordGpu must match std430 layout");

class TraceMaterialGpu
{
public:
    bool rebuild(std::string *error = nullptr);
    void shutdown();

    GLuint recordBuffer() const { return record_buffer_; }
    GLuint colorAtlas() const { return color_atlas_; }
    GLuint dataAtlas() const { return data_atlas_; }
    std::size_t materialCount() const { return material_count_; }
    std::size_t recordCount() const { return records_.size(); }
    std::uint64_t materialRevision() const { return material_revision_; }

private:
    struct PackedImage
    {
        Models::TextureHandle source = Models::INVALID_TEXTURE;
        Material::ColorSpace color_space = Material::ColorSpace::Linear;
        int layer = 0;
        int width = 0;
        int height = 0;
    };

    bool rebuildAtlases(std::string *error);
    bool uploadBuffer(const void *data, std::size_t size, std::string *error);
    int packedLayer(Models::TextureHandle source,
                    Material::ColorSpace color_space) const;

    GLuint record_buffer_ = 0;
    GLuint color_atlas_ = 0;
    GLuint data_atlas_ = 0;
    std::vector<TraceMaterialRecordGpu> records_;
    std::vector<PackedImage> packed_images_;
    std::size_t material_count_ = 0u;
    int color_page_width_ = 1;
    int color_page_height_ = 1;
    int data_page_width_ = 1;
    int data_page_height_ = 1;
    std::uint64_t material_revision_ = 0u;
};

} }
#endif
