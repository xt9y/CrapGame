#include "Renderer/Gpu/CacheStats.hpp"

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
    Gpu::CacheStats previous={};
    previous.radiance_queries=10u;
    previous.radiance_hits=4u;
    Gpu::CacheStats current=previous;
    current.static_shadow_cached+=2u;
    current.shadow_pages_requested+=9u;
    current.shadow_pages_cached+=7u;
    current.shadow_pages_rendered+=2u;
    current.reprojection_pixels+=64u;
    current.radiance_queries+=4u;
    current.radiance_hits+=3u;

    const Gpu::CacheStats delta=Gpu::cacheStatsDelta(current,previous);
    require(delta.static_shadow_cached==2u,"shadow cache delta");
    require(delta.shadow_pages_requested==9u&&delta.shadow_pages_cached==7u
            &&delta.shadow_pages_rendered==2u,"virtual shadow page delta");
    require(delta.reprojection_pixels==64u,"reprojection delta");
    require(delta.radiance_queries==4u&&delta.radiance_hits==3u,"radiance delta");
    require(Gpu::cacheHitPercent(delta.radiance_hits,delta.radiance_queries)==75.0,
            "radiance hit percent");
    require(Gpu::cacheHitPercent(0u,0u)==0.0,"zero-query hit percent");

    std::puts("cache_stats_contract=PASS");
    return 0;
}
