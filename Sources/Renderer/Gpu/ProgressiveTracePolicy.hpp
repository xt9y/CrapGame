#ifndef CRAPGAME_RENDERER_GPU_PROGRESSIVETRACEPOLICY_HPP
#define CRAPGAME_RENDERER_GPU_PROGRESSIVETRACEPOLICY_HPP

#include <cstdint>

namespace Renderer { namespace Gpu {

struct TraceSlice{std::uint32_t index=0u,count=1u;bool completes_sweep=true;};
struct ProgressiveTracePolicy{static constexpr std::uint32_t MOVING_SLICE_COUNT=64u,STATIONARY_SLICE_COUNT=32u;static constexpr std::uint32_t sliceCount(bool moving,bool invalidated){return invalidated?1u:(moving?MOVING_SLICE_COUNT:STATIONARY_SLICE_COUNT);}};
constexpr std::uint32_t traceSliceDispatchGroups(std::uint32_t rows,std::uint32_t slices){const std::uint32_t count=slices==0u?1u:slices;return (rows+count-1u)/count;}
constexpr std::uint32_t traceSliceItemBudget(std::uint32_t items,std::uint32_t slices){return traceSliceDispatchGroups(items,slices);}
class ProgressiveTraceState{public:void reset(){phase_=0u;active_slice_count_=0u;}TraceSlice next(bool moving,bool invalidated){const std::uint32_t count=ProgressiveTracePolicy::sliceCount(moving,invalidated);if(invalidated||active_slice_count_!=count){phase_=0u;active_slice_count_=count;}TraceSlice out{phase_,count,phase_+1u>=count};phase_=out.completes_sweep?0u:phase_+1u;return out;}private:std::uint32_t phase_=0u,active_slice_count_=0u;};

} }
#endif
