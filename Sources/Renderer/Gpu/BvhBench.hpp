#ifndef CRAPGAME_RENDERER_GPU_BVHBENCH_HPP
#define CRAPGAME_RENDERER_GPU_BVHBENCH_HPP

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

    return primitive_count > threshold;
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
