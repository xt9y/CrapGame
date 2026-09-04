#ifndef CRAPGAME_RENDERER_GPU_REFLECTIONCACHEPOLICY_HPP
#define CRAPGAME_RENDERER_GPU_REFLECTIONCACHEPOLICY_HPP

#include "Renderer/Gpu/ReprojectionPolicy.hpp"

#include <cmath>
#include <cstdint>

namespace Renderer
{
namespace Gpu
{

enum class ReflectionSource
{
    ScreenSpace,
    RadianceCache,
    PreviousHistory,
    ImportedTrace,
};

struct ReflectionHistoryValidation
{
    bool previous_valid=false;
    std::uint32_t current_material_id=0u;
    std::uint32_t previous_material_id=0u;
    float position_error=0.0f;
    float camera_distance=0.0f;
    float normal_dot=-1.0f;
    float current_roughness=1.0f;
    float previous_roughness=1.0f;
};

struct ReflectionCachePolicy
{
    static constexpr float NORMAL_DOT_MIN=0.96f;
    static constexpr float ROUGHNESS_DELTA_MAX=0.05f;
    static constexpr float ROUGH_CACHE_MIN=0.35f;

    static bool historyValid(const ReflectionHistoryValidation& sample)
    {
        if(!sample.previous_valid)return false;
        if(sample.current_material_id!=sample.previous_material_id)return false;
        if(sample.position_error>ReprojectionPolicy::positionTolerance(sample.camera_distance))return false;
        if(sample.normal_dot<NORMAL_DOT_MIN)return false;
        return std::fabs(sample.current_roughness-sample.previous_roughness)<=ROUGHNESS_DELTA_MAX;
    }

    static ReflectionSource select(bool screen_hit,bool radiance_hit,bool history_hit)
    {
        if(screen_hit)return ReflectionSource::ScreenSpace;
        if(radiance_hit)return ReflectionSource::RadianceCache;
        if(history_hit)return ReflectionSource::PreviousHistory;
        return ReflectionSource::ImportedTrace;
    }

    static bool useRadianceCache(float roughness)
    {
        return roughness>=ROUGH_CACHE_MIN;
    }

    static bool usePreviousHistory(float roughness)
    {
        return roughness<ROUGH_CACHE_MIN;
    }
};

} // namespace Gpu
} // namespace Renderer

#endif
