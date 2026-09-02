#ifndef CRAPGAME_RENDERER_LUMEN_RADIOSITY_HPP
#define CRAPGAME_RENDERER_LUMEN_RADIOSITY_HPP

#include "Renderer/Lumen/RadianceCache.hpp"
#include "Renderer/Lumen/SurfaceCache.hpp"

namespace Renderer 
{
namespace Lumen 
{

void updateRadiosity (
                SurfaceCache *surface_cache,
                const RadianceCache& radiance_cache,
                float feedback
        );

} // namespace Lumen
} // namespace Renderer

#endif
