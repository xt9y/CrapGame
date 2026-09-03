#include "Profiler.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>

namespace Renderer
{
namespace Gpu
{
namespace
{

constexpr std::uint64_t SAMPLE_INTERVAL = 64u;
constexpr std::uint64_t PRINT_CHECK_INTERVAL = 1024u;
constexpr std::uint64_t PRINT_INTERVAL_NS = 2000000000ull;
constexpr std::uint32_t WARMUP_SAMPLES = 1u;
constexpr std::uint32_t PERF_FLUSH_INTERVAL = 64u;

std::uint64_t monotonicNanoseconds ()
{
    using Clock = std::chrono::steady_clock;
    return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                    Clock::now().time_since_epoch()
                ).count()
        );
}

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

const char *metricName (Profiler::Pass pass)
{
    switch (pass)
    {
        case Profiler::Pass::Geometry:       return "geometry_ms";
        case Profiler::Pass::DirectLighting: return "direct_ms";
        case Profiler::Pass::LumenTrace:     return "lumen_trace_ms";
        case Profiler::Pass::LumenComposite: return "lumen_compose_ms";
        case Profiler::Pass::Present:        return "present_ms";
        case Profiler::Pass::Count:          break;
    }

    return "unknown_ms";
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
    sample_counts_.fill(0u);

    performance_mode_ = PerformanceMetrics::requested();
    performance_start_ns_ = monotonicNanoseconds();
    performance_warmup_ms_ = PerformanceMetrics::warmupMilliseconds();
    performance_duration_ms_ = PerformanceMetrics::durationMilliseconds();
    performance_samples_since_flush_ = 0u;
    performance_writer_ready_ =
        !performance_mode_ || performance_writer_.open();

    if (!performance_writer_ready_)
    {
        shutdown();
        return false;
    }

    initialized_ = true;
    return true;
}

void Profiler::shutdown ()
{
    if (initialized_)
    {
        /* Perf mode exits on a wall-clock boundary while timer-query results
         * can still be in flight. Resolve those slots before deleting query
         * objects so the final samples make it to RendererCheck. */
        collect(true);
        performance_writer_.flush();
    }

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

    performance_writer_.close();
    milliseconds_.fill(0.0);
    sample_counts_.fill(0u);
    performance_start_ns_ = 0;
    performance_warmup_ms_ = 0.0;
    performance_duration_ms_ = 0.0;
    performance_samples_since_flush_ = 0u;
    current_slot_ = -1;
    performance_mode_ = false;
    performance_writer_ready_ = false;
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

bool Profiler::performanceSampleAllowed (const Slot& slot) const
{
    if (!performance_mode_
            || !performance_writer_ready_
            || slot.submitted_ns < performance_start_ns_)
    {
        return false;
    }

    const double elapsed_ms =
        static_cast<double>(slot.submitted_ns - performance_start_ns_)
        / 1000000.0;

    if (elapsed_ms < performance_warmup_ms_)
    {
        return false;
    }

    if (performance_duration_ms_ > 0.0
            && elapsed_ms > performance_duration_ms_)
    {
        return false;
    }

    return true;
}

void Profiler::consumeSlot (Slot& slot, bool block)
{
    if (!slot.pending)
    {
        return;
    }

    if (!block && !slotReady(slot))
    {
        return;
    }

    const bool record_performance = performanceSampleAllowed(slot);
    double gpu_pipeline_ms = 0.0;
    bool measured_any = false;

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

        measured_any = true;
        gpu_pipeline_ms += sample_ms;
        ++sample_counts_[pass];

        if (record_performance)
        {
            performance_writer_.write(
                    metricName(static_cast<Pass>(pass)),
                    sample_ms
                );
        }

        /* Shader first-use/JIT and resource residency can make the first
         * timestamp sample dramatically slower than steady state. Keep
         * startup cost out of the interactive throughput EWMA. RendererCheck
         * has its own explicit wall-clock warmup and receives raw samples. */
        if (sample_counts_[pass] <= WARMUP_SAMPLES)
        {
            continue;
        }

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

    if (record_performance && measured_any)
    {
        performance_writer_.write("gpu_pipeline_ms", gpu_pipeline_ms);
        ++performance_samples_since_flush_;

        if (performance_samples_since_flush_ >= PERF_FLUSH_INTERVAL)
        {
            performance_writer_.flush();
            performance_samples_since_flush_ = 0u;
        }
    }

    slot.pending = false;
    slot.measured.fill(false);
}

void Profiler::collect (bool block)
{
    if (!initialized_)
    {
        return;
    }

    for (Slot& slot : slots_)
    {
        consumeSlot(slot, block);
    }
}

void Profiler::beginFrame (std::uint64_t frame_index)
{
    current_slot_ = -1;

    if (!initialized_)
    {
        return;
    }

    const std::uint64_t interval =
        performance_mode_ ? 1u : SAMPLE_INTERVAL;

    if (frame_index % interval != 0u)
    {
        return;
    }

    collect(false);

    const std::uint64_t sample_index = frame_index / interval;
    Slot& slot = slots_[
        static_cast<std::size_t>(sample_index % SLOT_COUNT)
    ];

    if (slot.pending)
    {
        return;
    }

    slot.frame = frame_index;
    slot.submitted_ns = monotonicNanoseconds();
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
    static std::uint64_t last_print_ns = 0;

    if (!initialized_
            || performance_mode_
            || frame_index == 0
            || frame_index % PRINT_CHECK_INTERVAL != 0u)
    {
        return;
    }

    const std::uint64_t now_ns = monotonicNanoseconds();

    if (last_print_ns != 0
            && now_ns - last_print_ns < PRINT_INTERVAL_NS)
    {
        return;
    }

    last_print_ns = now_ns;
    collect(false);

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
