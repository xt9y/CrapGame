#ifndef CRAPGAME_RENDERER_LUMEN_REFLECTION_POLICY_HPP
#define CRAPGAME_RENDERER_LUMEN_REFLECTION_POLICY_HPP

namespace Renderer
{
namespace Lumen
{

inline bool reflectionUsesDetailedTrace(float roughness)
{
    return !(roughness >= 0.35f);
}

template <typename HitSampler, typename MissSampler>
auto sampleReflectionRadiance(
            bool hit,
            HitSampler&& hit_sampler,
            MissSampler&& miss_sampler
    )
{
    if (hit)
    {
        return hit_sampler();
    }

    return miss_sampler();
}

} // namespace Lumen
} // namespace Renderer

#endif
