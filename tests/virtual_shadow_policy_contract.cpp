#include "Renderer/Gpu/VirtualShadowPolicy.hpp"

#include <cstdlib>
#include <iostream>

static void require(bool value,const char *message)
{
    if(!value){std::cerr<<message<<'\n';std::exit(1);}
}

int main()
{
    using namespace Renderer::Gpu;
    require(VirtualShadowPolicy::PAGE_SIZE==128,"VSM page size");
    require(VirtualShadowPolicy::LEVEL0_PAGES==128,"VSM level-0 page count");
    require(VirtualShadowPolicy::VIRTUAL_RESOLUTION==16384,"VSM virtual resolution");
    require(VirtualShadowPolicy::MAX_PHYSICAL_PAGES==2048,"VSM physical-page budget");
    require(VirtualShadowPolicy::RECEIVER_MASK_SIZE==8,"VSM receiver mask");
    require(VirtualShadowPolicy::FIRST_CLIPMAP_LEVEL==6&&VirtualShadowPolicy::LAST_CLIPMAP_LEVEL==22,"directional clipmap range");
    require(VirtualShadowPolicy::FIRST_COARSE_LEVEL==15&&VirtualShadowPolicy::LAST_COARSE_LEVEL==18,"coarse clipmap range");
    require(virtualShadowMipLevel(1.0f)==0,"unit footprint selects mip zero");
    require(virtualShadowMipLevel(8.0f)==3,"eight-texel footprint selects mip three");
    require(virtualShadowDynamicLodBias(1800)>0.0f,"pool pressure raises LOD bias");
    require(virtualShadowDynamicLodBias(1000)==0.0f,"healthy pool keeps full resolution");
    std::cout<<"virtual_shadow_policy_contract=PASS\n";
}
