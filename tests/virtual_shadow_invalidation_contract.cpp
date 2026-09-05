#include "Renderer/Gpu/VirtualShadowInvalidation.hpp"
#include "Renderer/Gpu/VirtualShadowInvalidationShader.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

static void require(bool value,const char *message)
{
    if(!value){std::cerr<<message<<'\n';std::exit(1);}
}

int main()
{
    using namespace Renderer::Gpu;

    const ShadowInvalidationRange range=shadowInvalidationRange(
        6,
        {-1.0f,-1.0f,-1.0f},
        {1.0f,1.0f,1.0f},
        {1.0f,0.0f,0.0f},
        {0.0f,1.0f,0.0f});
    require(range.minimum_x<=range.maximum_x,
            "shadow invalidation x range");
    require(range.minimum_y<=range.maximum_y,
            "shadow invalidation y range");

    const std::string shader=VIRTUAL_SHADOW_INVALIDATION_COMPUTE;
    require(shader.find("PAGE_DIRTY")!=std::string::npos,
            "invalidation must dirty cached pages");
    require(shader.find("PAGE_QUEUED")!=std::string::npos,
            "invalidation must release stale queue state");
    require(shader.find("uInvalidateLight")!=std::string::npos,
            "light changes need full-light invalidation");
    require(shader.find("shadowInvalidationRegions")!=std::string::npos,
            "caster changes need bounded invalidation regions");
    require(shader.find("pageWorldSize")!=std::string::npos,
            "invalidation must operate in stable world-page coordinates");

    std::cout<<"virtual_shadow_invalidation_contract=PASS\n";
}
