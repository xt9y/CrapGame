#include "Renderer/Gpu/ProgressiveTracePolicy.hpp"

#include <cstdlib>
#include <iostream>

using namespace Renderer::Gpu;

static void require(bool condition,const char *message)
{
    if(!condition){std::cerr<<"progressive_trace_budget_contract: "<<message<<'\n';std::exit(1);}
}

int main()
{
    require(ProgressiveTracePolicy::MOVING_SLICE_COUNT==64u,
            "moving trace budget must split a full trace into 64 slices");
    require(ProgressiveTracePolicy::STATIONARY_SLICE_COUNT==32u,
            "stationary trace budget must split a full trace into 32 slices");
    require(traceSliceDispatchGroups(95u,32u)==3u,
            "slice dispatch must bound workgroup rows without dropping tail rows");

    ProgressiveTraceState state;
    TraceSlice invalidated=state.next(false,true);
    require(invalidated.index==0u&&invalidated.count==1u&&invalidated.completes_sweep,
            "invalidated scene must establish one complete baseline sweep");

    TraceSlice moving=state.next(true,false);
    require(moving.index==0u&&moving.count==64u&&!moving.completes_sweep,
            "moving trace must begin a 64-slice sweep");
    for(std::uint32_t index=1u;index<64u;++index)
    {
        moving=state.next(true,false);
        require(moving.index==index,"moving slice phase skipped or repeated");
        require(moving.completes_sweep==(index==63u),
                "moving sweep completion is reported on the wrong slice");
    }

    TraceSlice stationary=state.next(false,false);
    require(stationary.index==0u&&stationary.count==32u&&!stationary.completes_sweep,
            "stationary mode change must restart a 32-slice sweep");

    std::cout<<"progressive_trace_budget_contract=PASS\n";
    return 0;
}
