#ifndef CRAPGAME_RENDERER_GPU_PRESENTER_POLICY_HPP
#define CRAPGAME_RENDERER_GPU_PRESENTER_POLICY_HPP

#include <cstdint>

namespace Renderer
{
namespace Gpu
{

inline bool glValidationEnabled (const char *value)
{
    return value && value[0] == '1' && value[1] == '\0';
}

inline bool presenterStateNeedsBind (
            bool state_valid,
            std::uint32_t texture,
            std::uint32_t cached_texture,
            bool flip_y,
            bool cached_flip_y)
{
    return !state_valid
        || texture != cached_texture
        || flip_y != cached_flip_y;
}

} // namespace Gpu
} // namespace Renderer

#endif
