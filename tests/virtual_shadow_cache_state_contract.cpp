#include "Renderer/Gpu/VirtualShadowMapShader.hpp"
#include "Renderer/Gpu/VirtualShadowFinishShader.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

static void require(bool value,const char *message)
{
    if(!value){std::cerr<<message<<'\n';std::exit(1);}
}

int main()
{
    const std::string mark=Renderer::Gpu::VIRTUAL_SHADOW_MARK_COMPUTE;
    const std::string finish=Renderer::Gpu::VIRTUAL_SHADOW_FINISH_CACHE_COMPUTE;

    require(mark.find("PAGE_QUEUED")!=std::string::npos,
            "dirty VSM pages need a queued state");
    require(mark.find("atomicOr(shadowPages[mapped].state.z,PAGE_QUEUED)")!=std::string::npos,
            "cached dirty pages must enqueue once");
    require(mark.find("PAGE_VALID|PAGE_DIRTY|PAGE_QUEUED")!=std::string::npos,
            "new VSM pages must start queued and dirty");
    require(finish.find("rasterOverflow")!=std::string::npos,
            "raster overflow must participate in page completion");
    require(finish.find("~PAGE_QUEUED")!=std::string::npos,
            "finish must release the queue state");
    require(finish.find("~(PAGE_DIRTY|PAGE_QUEUED)")!=std::string::npos,
            "successful pages must become clean");

    std::cout<<"virtual_shadow_cache_state_contract=PASS\n";
}
