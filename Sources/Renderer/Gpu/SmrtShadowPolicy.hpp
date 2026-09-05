#ifndef CRAPGAME_RENDERER_GPU_SMRTSHADOWPOLICY_HPP
#define CRAPGAME_RENDERER_GPU_SMRTSHADOWPOLICY_HPP

namespace Renderer
{
namespace Gpu
{

struct SmrtShadowPolicy
{
    static constexpr int DIRECTIONAL_RAYS = 7;
    static constexpr int DIRECTIONAL_SAMPLES_PER_RAY = 8;
    static constexpr int LOCAL_RAYS = 7;
    static constexpr int LOCAL_SAMPLES_PER_RAY = 8;
    static constexpr float DIRECTIONAL_RAY_LENGTH_SCALE = 1.5f;
    static constexpr float DIRECTIONAL_EXTRAPOLATE_MAX_SLOPE = 5.0f;
    static constexpr float LOCAL_EXTRAPOLATE_MAX_SLOPE = 0.05f;
    static constexpr bool ADAPTIVE_RAY_COUNT = true;
};

} // namespace Gpu
} // namespace Renderer

#endif
