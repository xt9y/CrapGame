#include "Renderer/Gpu/LumenGpu.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition,const char *message)
{
    if(condition)return;
    std::cerr<<"imported_trace_cadence_contract: "<<message<<'\n';
    std::exit(1);
}
}

int main()
{
    using Renderer::Gpu::importedTraceSampleDue;
    require(importedTraceSampleDue(false,1u),"stationary camera must trace when scheduled");
    require(importedTraceSampleDue(true,0u),"first moving sample must trace");
    require(!importedTraceSampleDue(true,1u),"moving sample one must reuse history");
    require(!importedTraceSampleDue(true,2u),"moving sample two must reuse history");
    require(importedTraceSampleDue(true,3u),"moving sample three must trace again");
    std::cout<<"imported_trace_cadence_contract=PASS\n";
    return 0;
}
