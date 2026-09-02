#include "Radiosity.hpp"

#include "Renderer/Math/Math.hpp"

#include <algorithm>

namespace Renderer 
{
namespace Lumen 
{

void updateRadiosity (
                SurfaceCache *surface_cache,
                const RadianceCache& radiance_cache,
                float feedback
        ) 
{
    if (!surface_cache) 
    {
        return;
    }

    const float amount = std::max(0.0f, std::min(1.0f, feedback));

    for (SurfaceSample& surface : surface_cache->samples()) 
    {
        const Math::Vec3 incoming =
            radiance_cache.sample(surface.card.position);

        const Math::Vec3 target =
            Math::multiply(
                    Math::multiply(surface.albedo, incoming),
                    0.55f
                );

        surface.indirect_lighting = Math::add(
                Math::multiply(
                        surface.indirect_lighting,
                        1.0f - amount
                    ),
                Math::multiply(target, amount)
            );
    }
}

} // namespace Lumen
} // namespace Renderer
