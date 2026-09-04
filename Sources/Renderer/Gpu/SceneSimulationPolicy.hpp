#ifndef CRAPGAME_RENDERER_GPU_SCENESIMULATIONPOLICY_HPP
#define CRAPGAME_RENDERER_GPU_SCENESIMULATIONPOLICY_HPP

namespace Renderer
{
namespace Gpu
{

inline bool& sceneSimulationEnabledStorage()
{
    static bool enabled=true;
    return enabled;
}

inline void setSceneSimulationEnabled(bool enabled)
{
    sceneSimulationEnabledStorage()=enabled;
}

inline bool sceneSimulationEnabled()
{
    return sceneSimulationEnabledStorage();
}

} // namespace Gpu
} // namespace Renderer

#endif
