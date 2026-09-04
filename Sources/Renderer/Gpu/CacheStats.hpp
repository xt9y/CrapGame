#ifndef CRAPGAME_RENDERER_GPU_CACHESTATS_HPP
#define CRAPGAME_RENDERER_GPU_CACHESTATS_HPP

#include <cstdint>

namespace Renderer
{
namespace Gpu
{

struct CacheStats
{
    std::uint64_t static_shadow_generated=0u;
    std::uint64_t static_shadow_cached=0u;
    std::uint64_t reprojection_pixels=0u;
    std::uint64_t dirty_tiles=0u;
    std::uint64_t reused_pixels=0u;
    std::uint64_t radiance_queries=0u;
    std::uint64_t radiance_hits=0u;
    std::uint64_t reflection_queries=0u;
    std::uint64_t reflection_hits=0u;
};

inline double cacheHitPercent(std::uint64_t hits,std::uint64_t queries)
{
    return queries==0u?0.0:100.0*static_cast<double>(hits)/static_cast<double>(queries);
}

inline CacheStats cacheStatsDelta(const CacheStats& current,const CacheStats& previous)
{
    const auto delta=[](std::uint64_t a,std::uint64_t b){return a>=b?a-b:0u;};
    CacheStats result;
    result.static_shadow_generated=delta(current.static_shadow_generated,previous.static_shadow_generated);
    result.static_shadow_cached=delta(current.static_shadow_cached,previous.static_shadow_cached);
    result.reprojection_pixels=delta(current.reprojection_pixels,previous.reprojection_pixels);
    result.dirty_tiles=delta(current.dirty_tiles,previous.dirty_tiles);
    result.reused_pixels=delta(current.reused_pixels,previous.reused_pixels);
    result.radiance_queries=delta(current.radiance_queries,previous.radiance_queries);
    result.radiance_hits=delta(current.radiance_hits,previous.radiance_hits);
    result.reflection_queries=delta(current.reflection_queries,previous.reflection_queries);
    result.reflection_hits=delta(current.reflection_hits,previous.reflection_hits);
    return result;
}

} // namespace Gpu
} // namespace Renderer

#endif
