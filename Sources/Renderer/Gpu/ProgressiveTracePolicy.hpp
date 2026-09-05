#ifndef CRAPGAME_RENDERER_GPU_PROGRESSIVETRACEPOLICY_HPP
#define CRAPGAME_RENDERER_GPU_PROGRESSIVETRACEPOLICY_HPP

#include <cstdint>

namespace Renderer
{
namespace Gpu
{

struct TraceSlice
{
    std::uint32_t index = 0u;
    std::uint32_t count = 1u;
    bool completes_sweep = true;
};

struct ProgressiveTracePolicy
{
    static constexpr std::uint32_t MOVING_SLICE_COUNT = 64u;
    static constexpr std::uint32_t STATIONARY_SLICE_COUNT = 32u;

    static constexpr std::uint32_t sliceCount (
            bool camera_moving,
            bool invalidated
        )
    {
        return invalidated
            ? 1u
            : (camera_moving ? MOVING_SLICE_COUNT : STATIONARY_SLICE_COUNT);
    }
};

constexpr std::uint32_t traceSliceDispatchGroups (
        std::uint32_t group_rows,
        std::uint32_t slice_count
    )
{
    const std::uint32_t count = slice_count == 0u ? 1u : slice_count;
    return (group_rows + count - 1u) / count;
}

constexpr std::uint32_t traceSliceItemBudget (
        std::uint32_t item_count,
        std::uint32_t slice_count
    )
{
    return traceSliceDispatchGroups(item_count, slice_count);
}

class ProgressiveTraceState
{
public:
    void reset ()
    {
        phase_ = 0u;
        active_slice_count_ = 0u;
    }

    TraceSlice next (bool camera_moving, bool invalidated)
    {
        const std::uint32_t count =
            ProgressiveTracePolicy::sliceCount(camera_moving, invalidated);

        if (invalidated || active_slice_count_ != count)
        {
            phase_ = 0u;
            active_slice_count_ = count;
        }

        TraceSlice slice;
        slice.index = phase_;
        slice.count = count;
        slice.completes_sweep = phase_ + 1u >= count;
        phase_ = slice.completes_sweep ? 0u : phase_ + 1u;
        return slice;
    }

private:
    std::uint32_t phase_ = 0u;
    std::uint32_t active_slice_count_ = 0u;
};

} // namespace Gpu
} // namespace Renderer

#endif
