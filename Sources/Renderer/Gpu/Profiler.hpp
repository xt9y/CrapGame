#ifndef CRAPGAME_RENDERER_GPU_PROFILER_HPP
#define CRAPGAME_RENDERER_GPU_PROFILER_HPP

#include <lwcgl/lwcgl.h>
#include <lwcgl/glmodern.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace Renderer
{
namespace Gpu
{

class Profiler
{
public:
    enum class Pass : std::size_t
    {
        Geometry = 0,
        DirectLighting,
        Lumen,
        Present,
        Count,
    };

    bool init ();
    void shutdown ();

    void beginFrame (std::uint64_t frame_index);
    void begin (Pass pass);
    void end (Pass pass);
    void endFrame ();

    void printIfDue (std::uint64_t frame_index);

    double milliseconds (Pass pass) const;
    double totalMilliseconds () const;

private:
    static constexpr std::size_t PASS_COUNT =
        static_cast<std::size_t>(Pass::Count);
    static constexpr std::size_t SLOT_COUNT = 8u;

    struct Slot
    {
        std::array<GLuint, PASS_COUNT> begin = {};
        std::array<GLuint, PASS_COUNT> end = {};
        std::array<bool, PASS_COUNT> measured = {};
        std::uint64_t frame = 0;
        bool pending = false;
    };

    void collect ();
    bool slotReady (const Slot& slot) const;

    std::array<Slot, SLOT_COUNT> slots_ = {};
    std::array<double, PASS_COUNT> milliseconds_ = {};

    int current_slot_ = -1;
    bool initialized_ = false;
};

} // namespace Gpu
} // namespace Renderer

#endif
