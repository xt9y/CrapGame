#include "Renderer/Gpu/ScenePrewarm.hpp"

#include "Renderer/Gpu/DirectLightingGpu.hpp"
#include "Renderer/Gpu/GBufferGpu.hpp"
#include "Renderer/Gpu/LumenGpu.hpp"

namespace Renderer
{
namespace Gpu
{

bool ScenePrewarm::run (
        const Ecs::World& world,
        GBufferGpu& gbuffer,
        DirectLightingGpu& direct,
        LumenGpu& lumen,
        int width,
        int height,
        std::string *error)
{
    return runWith(
        world,
        gbuffer,
        direct,
        lumen,
        width,
        height,
        error
    );
}

} // namespace Gpu
} // namespace Renderer
