#ifndef CRAPGAME_RENDERER_GPU_VIRTUALSHADOWPOLICY_HPP
#define CRAPGAME_RENDERER_GPU_VIRTUALSHADOWPOLICY_HPP

#include <algorithm>
#include <cmath>

namespace Renderer
{
namespace Gpu
{

struct VirtualShadowPolicy
{
    static constexpr int PAGE_SIZE = 128;
    static constexpr int LEVEL0_PAGES = 128;
    static constexpr int VIRTUAL_RESOLUTION = PAGE_SIZE * LEVEL0_PAGES;
    static constexpr int MAX_MIP_LEVELS = 8;
    static constexpr int MAX_PHYSICAL_PAGES = 2048;
    static constexpr int RECEIVER_MASK_SIZE = 8;
    static constexpr int FIRST_CLIPMAP_LEVEL = 6;
    static constexpr int LAST_CLIPMAP_LEVEL = 22;
    static constexpr int FIRST_COARSE_LEVEL = 15;
    static constexpr int LAST_COARSE_LEVEL = 18;
    static constexpr int MAX_PAGE_AGE = 1000;
    static constexpr float Z_RANGE_SCALE = 1000.0f;
    static constexpr float PAGE_PRESSURE_THRESHOLD = 0.85f;
    static constexpr float MAX_DYNAMIC_LOD_BIAS = 2.0f;
    static constexpr float NORMAL_BIAS = 0.5f;
    static constexpr float SCREEN_RAY_LENGTH = 0.015f;
};

inline int virtualShadowMipLevel (float footprint)
{
    const float value = std::max(1.0f, footprint);
    const int level = static_cast<int>(std::floor(std::log2(value)));

    return std::max(
            0,
            std::min(VirtualShadowPolicy::MAX_MIP_LEVELS - 1, level)
        );
}

inline float virtualShadowClipmapExtent (int level)
{
    return std::exp2(static_cast<float>(level));
}

inline float virtualShadowDynamicLodBias (int requested_pages)
{
    const float threshold =
        static_cast<float>(VirtualShadowPolicy::MAX_PHYSICAL_PAGES) *
        VirtualShadowPolicy::PAGE_PRESSURE_THRESHOLD;

    if (static_cast<float>(requested_pages) <= threshold)
    {
        return 0.0f;
    }

    const float maximum =
        static_cast<float>(VirtualShadowPolicy::MAX_PHYSICAL_PAGES);

    const float pressure =
        (static_cast<float>(requested_pages) - threshold) /
        std::max(1.0f, maximum - threshold);

    return std::min(
            VirtualShadowPolicy::MAX_DYNAMIC_LOD_BIAS,
            std::max(
                    0.0f,
                    pressure * VirtualShadowPolicy::MAX_DYNAMIC_LOD_BIAS
                )
        );
}

} // namespace Gpu
} // namespace Renderer

#endif
