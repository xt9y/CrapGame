#ifndef CRAPGAME_RENDERER_GPU_SCENEPREWARM_HPP
#define CRAPGAME_RENDERER_GPU_SCENEPREWARM_HPP

#include "Ecs/Ecs.hpp"

#include <string>

namespace Renderer
{
namespace Gpu
{

class GBufferGpu;
class DirectLightingGpu;
class LumenGpu;

class ScenePrewarm
{
public:
    bool run (
        const Ecs::World& world,
        GBufferGpu& gbuffer,
        DirectLightingGpu& direct,
        LumenGpu& lumen,
        int width,
        int height,
        std::string *error = nullptr
    );

    template <typename GBuffer, typename Direct, typename Lumen>
    bool runWith (
        const Ecs::World& world,
        GBuffer& gbuffer,
        Direct& direct,
        Lumen& lumen,
        int width,
        int height,
        std::string *error = nullptr
    )
    {
        complete_ = false;

        if (!gbuffer.resize(width, height, error)
                || !direct.resize(width, height, error)
                || !lumen.resize(width, height, error)
                || !gbuffer.prewarm(world, error)
                || !direct.prewarm(world, error)
                || !lumen.prewarmImportedTrace(error))
        {
            return false;
        }

        complete_ = true;
        if (error) error->clear();
        return true;
    }

    bool complete () const { return complete_; }
    void reset () { complete_ = false; }

private:
    bool complete_ = false;
};

} // namespace Gpu
} // namespace Renderer

#endif