#include "Renderer/Gpu/DirtyTileShader.hpp"
#include "Renderer/Gpu/LumenImportedStageAShader.hpp"
#include "Renderer/Gpu/ProgressiveTracePolicy.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

using namespace Renderer::Gpu;

static void require(bool condition,const char *message)
{
    if(!condition){std::cerr<<"progressive_trace_budget_contract: "<<message<<'\n';std::exit(1);}
}

int main(int argc,char **argv)
{
    require(ProgressiveTracePolicy::MOVING_SLICE_COUNT==64u,
            "moving trace budget must split a full trace into 64 slices");
    require(ProgressiveTracePolicy::STATIONARY_SLICE_COUNT==32u,
            "stationary trace budget must split a full trace into 32 slices");
    require(traceSliceDispatchGroups(95u,32u)==3u,
            "slice dispatch must bound workgroup rows without dropping tail rows");
    require(traceSliceItemBudget(1000u,64u)==16u,
            "moving dirty-tile work budget is not bounded to roughly 1/64");

    const TraceSlice invalidated=traceSliceForFrame(29u,false,true);
    require(invalidated.index==0u&&invalidated.count==1u&&invalidated.completes_sweep,
            "invalidated scene must establish one complete baseline sweep");
    const TraceSlice moving=traceSliceForFrame(65u,true,false);
    require(moving.index==1u&&moving.count==64u&&!moving.completes_sweep,
            "moving slice phase must follow the secondary sample index");
    const TraceSlice stationary=traceSliceForFrame(63u,false,false);
    require(stationary.index==31u&&stationary.count==32u&&stationary.completes_sweep,
            "stationary sweep completion is reported on the wrong slice");

    ProgressiveTraceState state;
    TraceSlice stateSlice=state.next(true,false);
    require(stateSlice.index==0u&&stateSlice.count==64u,
            "stateful progressive sweep must begin at zero");
    for(std::uint32_t index=1u;index<64u;++index)
        stateSlice=state.next(true,false);
    require(stateSlice.completes_sweep,
            "stateful moving sweep never completes");

    const std::string dirty=DIRTY_TILE_COMPACT_COMPUTE;
    require(dirty.find("uSliceIndex")!=std::string::npos
            &&dirty.find("uSliceCount")!=std::string::npos,
            "dirty tile compaction is not slice-aware");
    require(dirty.find("linearTile")!=std::string::npos
            &&dirty.find("linearTile%sliceCount")!=std::string::npos,
            "dirty tile slice selection is not spatially distributed");

    const std::string lumen=lumenImportedStageATraceShader();
    require(lumen.find("traceDispatchMode<0")!=std::string::npos,
            "stationary Lumen trace does not encode progressive dispatch mode");
    require(lumen.find("logicalGroupY")!=std::string::npos,
            "stationary Lumen trace does not remap workgroup rows by slice");
    require(lumen.find("uint(uFrameIndex)%traceSliceCount")!=std::string::npos,
            "Lumen trace slice phase is not deterministic from the sample index");

    if(argc>1){std::ofstream out(argv[1]);out<<dirty;}
    if(argc>2){std::ofstream out(argv[2]);out<<lumen;}

    std::cout<<"progressive_trace_budget_contract=PASS\n";
    return 0;
}
