#ifndef CRAPGAME_RENDERER_GPU_LUMENSCHEDULE_HPP
#define CRAPGAME_RENDERER_GPU_LUMENSCHEDULE_HPP

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>

namespace Renderer
{
namespace Gpu
{

class LumenSchedule
{
public:
    explicit LumenSchedule (std::uint32_t fixed_hz = configuredFixedHz())
        : fixed_hz_(fixed_hz)
    {
    }

    void reset ()
    {
        initialized_ = false;
        stable_since_ns_ = 0u;
        next_update_ns_ = 0u;
    }

    bool due (std::uint64_t now_ns, bool scene_changed)
    {
        if (!initialized_)
        {
            initialized_ = true;
            stable_since_ns_ = now_ns;
            next_update_ns_ = 0u;
        }

        if (scene_changed)
        {
            stable_since_ns_ = now_ns;
            next_update_ns_ = 0u;
        }

        return next_update_ns_ == 0u || now_ns >= next_update_ns_;
    }

    void markUpdated (std::uint64_t now_ns)
    {
        if (!initialized_)
        {
            initialized_ = true;
            stable_since_ns_ = now_ns;
        }

        const std::uint32_t hz = currentHz(now_ns);
        next_update_ns_ = now_ns + intervalNanoseconds(hz);
    }

    std::uint32_t currentHz (std::uint64_t now_ns) const
    {
        if (fixed_hz_ != 0u)
        {
            return fixed_hz_;
        }

        if (!initialized_ || now_ns <= stable_since_ns_)
        {
            return 60u;
        }

        const std::uint64_t stable_ns = now_ns - stable_since_ns_;

        if (stable_ns < 250000000ull)
        {
            return 60u;
        }

        if (stable_ns < 1000000000ull)
        {
            return 30u;
        }

        if (stable_ns < 3000000000ull)
        {
            return 20u;
        }

        return 15u;
    }

    std::uint32_t fixedHz () const
    {
        return fixed_hz_;
    }

    static std::uint32_t configuredFixedHz ()
    {
        const char *value = std::getenv("CRAPGAME_LUMEN_HZ");

        if (!value || !*value)
        {
            return 0u;
        }

        char *end = nullptr;
        errno = 0;
        const unsigned long parsed = std::strtoul(value, &end, 10);

        if (errno != 0
                || end == value
                || *end != '\0'
                || parsed == 0ul
                || parsed > 1000ul)
        {
            return 0u;
        }

        return static_cast<std::uint32_t>(parsed);
    }

private:
    static std::uint64_t intervalNanoseconds (std::uint32_t hz)
    {
        return 1000000000ull / static_cast<std::uint64_t>(hz == 0u ? 1u : hz);
    }

    std::uint64_t stable_since_ns_ = 0u;
    std::uint64_t next_update_ns_ = 0u;
    std::uint32_t fixed_hz_ = 0u;
    bool initialized_ = false;
};

} // namespace Gpu
} // namespace Renderer

#endif
