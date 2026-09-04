#ifndef CRAPGAME_RENDERER_GPU_MATERIALGPU_HPP
#define CRAPGAME_RENDERER_GPU_MATERIALGPU_HPP

#include "Renderer/Material/Material.hpp"

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Renderer
{
namespace Gpu
{

class MaterialGpu
{
public:
    static constexpr std::size_t LIVE_TEXTURE_COUNT = 15u;

    bool init(std::string *error = nullptr);
    bool ensure(Material::MaterialHandle material, std::string *error = nullptr);
    bool bind(Material::MaterialHandle material, GLuint first_unit,
              std::string *error = nullptr);
    std::uint32_t textureMask(Material::MaterialHandle material) const;
    void shutdown();

    static Material::Slot liveSlot(std::size_t index);

private:
    struct TextureGpu
    {
        Models::TextureHandle source = Models::INVALID_TEXTURE;
        Material::ColorSpace color_space = Material::ColorSpace::Linear;
        bool normal_map = false;
        bool clamp = false;
        GLuint texture = 0;
    };

    GLuint ensureTexture(const Material::TextureBinding& binding,
                         Material::Slot slot,
                         std::string *error);
    GLuint findTexture(const Material::TextureBinding& binding,
                       Material::Slot slot) const;

    std::vector<TextureGpu> textures_;
    bool initialized_ = false;
};

} // namespace Gpu
} // namespace Renderer

#endif
