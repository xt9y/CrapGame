#include "Profiler.hpp"

#include <algorithm>
#include <cstdio>

namespace Renderer
{
namespace Gpu
{
namespace
{

constexpr std::uint64_t SAMPLE_INTERVAL = 64u;
constexpr std::uint64_t PRINT_INTERVAL = 4096u;

const char *passName (Profiler::Pass pass)
{
    switch (pass)
    {
        case Profiler::Pass::Geometry:       return "geometry";
        case Profiler::Pass::DirectLighting: return "direct";
        case Profiler::Pass::LumenTrace:     return "lumen-trace";
        case Profiler::Pass::LumenComposite: return "lumen-compose";
        case Profiler::Pass::Present:        return "present";
        case Profiler::Pass::Count:          break;
    }

    return "unknown";
}

} // namespace

bool Profiler::init ()
{
    shutdown();

    if (!GL33.glGenQueries
            || !GL33.glDeleteQueries
            || !GL33.glQueryCounter
            || !GL33.glGetQueryObjectiv
            || !GL33.glGetQueryObjectui64v)
    {
        return false;
    }

    for (Slot& slot : slots_)
    {
        GL33.glGenQueries(
                static_cast<GLsizei>(PASS_COUNT),
                slot.begin.data()
            );
        GL33.glGenQueries(
                static_cast<GLsizei>(PASS_COUNT),
                slot.end.data()
            );

        for (std::size_t pass = 0; pass < PASS_COUNT; ++pass)
        {
            if (slot.begin[pass] == 0 || slot.end[pass] == 0)
            {
                shutdown();
                return false;
            }
        }
    }

    milliseconds_.fill(0.0);
    initialized_ = true;
    return true;
}

void Profiler::shutdown ()
{
    for (Slot& slot : slots_)
    {
        if (slot.begin[0] != 0)
        {
            GL33.glDeleteQueries(
                    static_cast<GLsizei>(PASS_COUNT),
                    slot.begin.data()
                );
        }

        if (slot.end[0] != 0)
        {
            GL33.glDeleteQueries(
                    static_cast<GLsizei>(PASS_COUNT),
                    slot.end.data()
                );
        }

        slot = {};
    }

    milliseconds_.fill(0.0);
    current_slot_ = -1;
    initialized_ = false;
}

bool Profiler::slotReady (const Slot& slot) const
{
    if (!slot.pending)
    {
        return true;
    }

    for (std::size_t pass = 0; pass < PASS_COUNT; ++pass)
    {
        if (!slot.measured[pass])
        {
            continue;
        }

        GLint available = GL_FALSE;
        GL33.glGetQueryObjectiv(
                slot.end[pass],
                GL_QUERY_RESULT_AVAILABLE,
                &available
            );

        if (available != GL_TRUE)
        {
            return false;
        }
    }

    return true;
}

void Profiler::collect ()
{
    if (!initialized_)
    {
        return;
    }

    for (Slot& slot : slots_)
    {
        if (!slot.pending || !slotReady(slot))
        {
            continue;
        }

        for (std::size_t pass = 0; pass < PASS_COUNT; ++pass)
        {
            if (!slot.measured[pass])
            {
                continue;
            }

            LWCGLGLuint64 begin = 0;
            LWCGLGLuint64 end = 0;

            GL33.glGetQueryObjectui64v(
                    slot.begin[pass],
                    GL_QUERY_RESULT,
                    &begin
                );
            GL33.glGetQueryObjectui64v(
                    slot.end[pass],
                    GL_QUERY_RESULT,
                    &end
                );

            if (end < begin)
            {
                continue;
            }

            const double sample_ms =
                static_cast<double>(end - begin) / 1000000.0;

            if (milliseconds_[pass] <= 0.0)
            {
                milliseconds_[pass] = sample_ms;
            }
            else
            {
                constexpr double HISTORY_WEIGHT = 0.90;
                milliseconds_[pass] =
                    milliseconds_[pass] * HISTORY_WEIGHT +
                    sample_ms * (1.0 - HISTORY_WEIGHT);
            }
        }

        slot.pending = false;
        slot.measured.fill(false);
    }
}

void Profiler::beginFrame (std::uint64_t frame_index)
{
    current_slot_ = -1;

    if (!initialized_
            || frame_index % SAMPLE_INTERVAL != 0u)
    {
        return;
    }

    collect();

    const std::uint64_t sample_index = frame_index / SAMPLE_INTERVAL;
    Slot& slot = slots_[
        static_cast<std::size_t>(sample_index % SLOT_COUNT)
    ];

    if (slot.pending)
    {
        return;
    }

    slot.frame = frame_index;
    slot.measured.fill(false);
    current_slot_ = static_cast<int>(sample_index % SLOT_COUNT);
}

void Profiler::begin (Pass pass)
{
    if (current_slot_ < 0)
    {
        return;
    }

    const std::size_t index = static_cast<std::size_t>(pass);

    if (index >= PASS_COUNT)
    {
        return;
    }

    Slot& slot = slots_[static_cast<std::size_t>(current_slot_)];
    GL33.glQueryCounter(slot.begin[index], GL_TIMESTAMP);
    slot.measured[index] = true;
}

void Profiler::end (Pass pass)
{
    if (current_slot_ < 0)
    {
        return;
    }

    const std::size_t index = static_cast<std::size_t>(pass);

    if (index >= PASS_COUNT)
    {
        return;
    }

    Slot& slot = slots_[static_cast<std::size_t>(current_slot_)];

    if (!slot.measured[index])
    {
        return;
    }

    GL33.glQueryCounter(slot.end[index], GL_TIMESTAMP);
}

void Profiler::endFrame ()
{
    if (current_slot_ < 0)
    {
        return;
    }

    slots_[static_cast<std::size_t>(current_slot_)].pending = true;
    current_slot_ = -1;
}

void Profiler::printIfDue (std::uint64_t frame_index)
{
    if (!initialized_
            || frame_index == 0
            || frame_index % PRINT_INTERVAL != 0u)
    {
        return;
    }

    collect();

    std::fprintf(stderr, "GPU frame");

    for (std::size_t index = 0; index < PASS_COUNT; ++index)
    {
        std::fprintf(
                stderr,
                "  %s %.3f ms",
                passName(static_cast<Pass>(index)),
                milliseconds_[index]
            );
    }

    std::fprintf(
            stderr,
            "  total %.3f ms\n",
            totalMilliseconds()
        );
}

double Profiler::milliseconds (Pass pass) const
{
    const std::size_t index = static_cast<std::size_t>(pass);
    return index < PASS_COUNT ? milliseconds_[index] : 0.0;
}

double Profiler::totalMilliseconds () const
{
    double total = 0.0;

    for (double value : milliseconds_)
    {
        total += value;
    }

    return total;
}

} // namespace Gpu
} // namespace Renderer
