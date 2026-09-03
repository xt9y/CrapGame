#ifndef CRAPGAME_RENDERER_GPU_BVHBENCH_HPP
#define CRAPGAME_RENDERER_GPU_BVHBENCH_HPP

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>

namespace Renderer
{
namespace Gpu
{

enum class BvhMode
{
    Auto,
    Linear,
    Bvh,
};

struct BvhBenchConfig
{
    BvhMode mode = BvhMode::Auto;
    std::size_t stress_primitives = 0u;
};

inline BvhBenchConfig bvhBenchConfig ()
{
    BvhBenchConfig config;

    if (const char *mode = std::getenv("CRAPGAME_BVH_MODE"))
    {
        if (std::strcmp(mode, "linear") == 0)
        {
            config.mode = BvhMode::Linear;
        }
        else if (std::strcmp(mode, "bvh") == 0)
        {
            config.mode = BvhMode::Bvh;
        }
    }

    if (const char *stress = std::getenv("CRAPGAME_BVH_STRESS"))
    {
        char *end = nullptr;
        const unsigned long long parsed = std::strtoull(stress, &end, 10);

        if (end != stress && *end == '\0')
        {
            constexpr unsigned long long MAX_STRESS_PRIMITIVES = 100000ull;
            config.stress_primitives = static_cast<std::size_t>(
                    parsed > MAX_STRESS_PRIMITIVES
                        ? MAX_STRESS_PRIMITIVES
                        : parsed
                );
        }
    }

    return config;
}

inline std::size_t configuredBvhAutoThreshold (std::size_t minimum_threshold)
{
    /* The corrected 66-primitive benchmark is decisively in BVH territory,
     * but we do not have enough samples yet to justify enabling tree
     * traversal immediately above the old 8-primitive guess. Keep the normal
     * path conservative until the crossover sweep refines this value. */
    constexpr std::size_t CONSERVATIVE_THRESHOLD = 32u;
    std::size_t threshold = std::max(minimum_threshold, CONSERVATIVE_THRESHOLD);

    if (const char *value = std::getenv("CRAPGAME_BVH_THRESHOLD"))
    {
        char *end = nullptr;
        const unsigned long long parsed = std::strtoull(value, &end, 10);

        if (end != value && *end == '\0' && parsed <= 100000ull)
        {
            threshold = static_cast<std::size_t>(parsed);
        }
    }

    return threshold;
}

inline bool shouldUseBvh (
            BvhMode mode,
            std::size_t primitive_count,
            std::size_t threshold = 8u
    )
{
    if (mode == BvhMode::Linear)
    {
        return false;
    }

    if (mode == BvhMode::Bvh)
    {
        return primitive_count > 0u;
    }

    return primitive_count > configuredBvhAutoThreshold(threshold);
}

inline const char *bvhModeName (BvhMode mode)
{
    switch (mode)
    {
        case BvhMode::Linear: return "linear";
        case BvhMode::Bvh:    return "bvh";
        case BvhMode::Auto:   return "auto";
    }

    return "auto";
}

} // namespace Gpu
} // namespace Renderer

#endif
