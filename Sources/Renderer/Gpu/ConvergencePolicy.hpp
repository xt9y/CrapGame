#ifndef CRAPGAME_RENDERER_GPU_CONVERGENCEPOLICY_HPP
#define CRAPGAME_RENDERER_GPU_CONVERGENCEPOLICY_HPP

#include <cerrno>
#include <cstdint>
#include <cstdlib>

namespace Renderer
{
namespace Gpu
{

struct ConvergencePolicy
{
    static constexpr std::uint32_t MIN_SAMPLES = 4u;
    static constexpr std::uint32_t DEFAULT_SAMPLES = 8u;
    static constexpr std::uint32_t MAX_SAMPLES = 16u;

    static std::uint32_t configuredSamples ()
    {
        const char *value = std::getenv("CRAPGAME_LUMEN_CONVERGENCE_SAMPLES");
        if (!value || !*value) return DEFAULT_SAMPLES;

        char *end = nullptr;
        errno = 0;
        const unsigned long parsed = std::strtoul(value, &end, 10);
        if (errno != 0 || end == value || *end != '\0' || parsed == 0ul)
            return DEFAULT_SAMPLES;
        if (parsed > MAX_SAMPLES) return MAX_SAMPLES;
        return static_cast<std::uint32_t>(parsed);
    }
};

} // namespace Gpu
} // namespace Renderer

#endif
