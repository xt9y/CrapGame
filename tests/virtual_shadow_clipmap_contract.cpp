#include "Renderer/Gpu/VirtualShadowMapGpu.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>

static void require(bool value,const char *message)
{
    if(!value){std::cerr<<message<<'\n';std::exit(1);}
}

int main()
{
    using namespace Renderer;
    using namespace Renderer::Gpu;

    const VirtualShadowClipmap a=
        directionalShadowClipmap(6,{0.0f,0.0f,0.0f},{0.0f,-1.0f,0.0f});
    const VirtualShadowClipmap b=
        directionalShadowClipmap(7,{0.0f,0.0f,0.0f},{0.0f,-1.0f,0.0f});
    require(std::fabs(b.extent-a.extent*2.0f)<0.001f,
            "clipmap coverage doubles");

    const VirtualShadowClipmap c=directionalShadowClipmap(
            6,
            {a.texel_world_size*32.0f,0.0f,0.0f},
            {0.0f,-1.0f,0.0f}
        );
    require(c.page_offset_x==a.page_offset_x&&c.page_offset_y==a.page_offset_y,
            "sub-page movement keeps cached page translation");

    const VirtualShadowClipmap d=directionalShadowClipmap(
            6,
            {a.texel_world_size*160.0f,0.0f,0.0f},
            {0.0f,-1.0f,0.0f}
        );
    require(d.page_offset_x!=a.page_offset_x||d.page_offset_y!=a.page_offset_y,
            "whole-page movement translates clipmap pages");

    std::cout<<"virtual_shadow_clipmap_contract=PASS\n";
}
