#ifndef CRAPGAME_RENDERER_GPU_STATICDIRECTSPLITPOLICY_HPP
#define CRAPGAME_RENDERER_GPU_STATICDIRECTSPLITPOLICY_HPP

#include "Renderer/Gpu/RevisionState.hpp"

namespace Renderer
{
namespace Gpu
{

inline bool staticDiffuseValid(
            const RevisionState& cached,
            const RevisionState& current)
{
    return worldRadianceValid(cached,current);
}

inline bool viewSpecularValid(
            const RevisionState& cached,
            const RevisionState& current)
{
    return sameFrameInputs(cached,current);
}

} // namespace Gpu
} // namespace Renderer

#endif
