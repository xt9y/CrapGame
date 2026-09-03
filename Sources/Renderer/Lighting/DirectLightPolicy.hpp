#ifndef CRAPGAME_RENDERER_LIGHTING_DIRECT_LIGHT_POLICY_HPP
#define CRAPGAME_RENDERER_LIGHTING_DIRECT_LIGHT_POLICY_HPP

#include "Ecs/Ecs.hpp"

namespace Renderer
{
namespace Lighting
{

inline bool directLightSampleIsPositionIndependent(Ecs::LightType type)
{
    return type == Ecs::LightType::Directional;
}

} // namespace Lighting
} // namespace Renderer

#endif
