#ifndef CRAPGAME_RENDERER_GPU_SHADOWPAGECACHEPOLICY_HPP
#define CRAPGAME_RENDERER_GPU_SHADOWPAGECACHEPOLICY_HPP

#include <cstdint>
#include <limits>
#include <vector>

namespace Renderer
{
namespace Gpu
{

struct ShadowPageKey
{
    std::uint32_t light = 0u;
    std::uint16_t level = 0u;
    std::uint16_t mip = 0u;
    std::int32_t x = 0;
    std::int32_t y = 0;
};

struct ShadowPageState
{
    ShadowPageKey key = {};
    std::uint32_t physical = 0u;
    std::uint64_t last_requested = 0u;
    std::uint64_t revision = 0u;
    bool allocated = false;
    bool dirty = false;
    bool dynamic = false;
};

inline bool sameShadowPageKey (
            const ShadowPageKey& a,
            const ShadowPageKey& b
    )
{
    return a.light == b.light
        && a.level == b.level
        && a.mip == b.mip
        && a.x == b.x
        && a.y == b.y;
}

inline int chooseShadowPageEviction (
            const std::vector<ShadowPageState>& pages,
            std::uint64_t frame_index
    )
{
    int selected = -1;
    std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();

    for (std::size_t index = 0; index < pages.size(); ++index)
    {
        const ShadowPageState& page = pages[index];
        if (!page.allocated || page.last_requested == frame_index)
        {
            continue;
        }

        if (selected < 0 || page.last_requested < oldest)
        {
            selected = static_cast<int>(index);
            oldest = page.last_requested;
        }
    }

    return selected;
}

} // namespace Gpu
} // namespace Renderer

#endif
