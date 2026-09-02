#ifndef CRAPGAME_RENDERER_LUMEN_SCENELIGHTING_HPP
#define CRAPGAME_RENDERER_LUMEN_SCENELIGHTING_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Lumen/SurfaceCache.hpp"
#include "Renderer/Shadows/Shadows.hpp"

namespace Renderer 
{
namespace Lumen 
{

void updateSceneLighting (
                SurfaceCache *surface_cache,
                const Ecs::World& world,
                const Shadows::Scene& shadows
        );

} // namespace Lumen
} // namespace Renderer

#endif
