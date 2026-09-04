#include "Renderer/Gpu/SurfaceFormats.hpp"

#include <cstdio>
#include <cstdlib>

using namespace Renderer;

static void require(bool value,const char *message)
{
    if(value)return;
    std::fprintf(stderr,"FAIL: %s\n",message);
    std::exit(1);
}

int main()
{
    require(Gpu::GBUFFER_STAGE_A_SAVED_BYTES_PER_PIXEL==8u,
            "Stage A must remove one RGBA16F position attachment");
    require(Gpu::GBUFFER_STAGE_A_BYTES_PER_PIXEL<Gpu::GBUFFER_LEGACY_BYTES_PER_PIXEL,
            "Stage A must reduce per-pixel GBuffer bandwidth");
    require(Gpu::gbufferStageASavedBytes(1280u,870u)==1280u*870u*8u,
            "frame savings must scale exactly with pixel count");

    std::puts("gbuffer_bandwidth_contract=PASS");
    return 0;
}
