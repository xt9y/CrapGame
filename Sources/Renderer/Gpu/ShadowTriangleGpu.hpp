#ifndef CRAPGAME_RENDERER_GPU_SHADOWTRIANGLEGPU_HPP
#define CRAPGAME_RENDERER_GPU_SHADOWTRIANGLEGPU_HPP

#include <cstddef>

namespace Renderer
{
namespace Gpu
{

struct ShadowTriangleGpu
{
    float p0[4]={};
    float p1[4]={};
    float p2[4]={};
};

static_assert(sizeof(ShadowTriangleGpu)==48u,
              "ShadowTriangleGpu must stay compact and std430-compatible");

} // namespace Gpu
} // namespace Renderer

#endif
