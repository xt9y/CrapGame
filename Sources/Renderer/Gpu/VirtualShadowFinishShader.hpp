#ifndef CRAPGAME_RENDERER_GPU_VIRTUALSHADOWFINISHSHADER_HPP
#define CRAPGAME_RENDERER_GPU_VIRTUALSHADOWFINISHSHADER_HPP

namespace Renderer
{
namespace Gpu
{

constexpr const char *VIRTUAL_SHADOW_FINISH_CACHE_COMPUTE=R"GLSL(
#version 430 core
layout(local_size_x=64,local_size_y=1,local_size_z=1) in;
#define PAGE_DIRTY 2u
#define PAGE_QUEUED 8u
struct ShadowPhysicalPage{ivec4 key;uvec4 state;};
struct ShadowDirtyPage{uvec4 data;};
struct DrawArraysIndirectCommand
{
    uint count;
    uint instanceCount;
    uint first;
    uint baseInstance;
};
layout(std430,binding=0) buffer ShadowPageMetadata{ShadowPhysicalPage shadowPages[];};
layout(std430,binding=1) readonly buffer ShadowDirtyPages
{
    uint dirtyPageCount;
    uint dirtyPadding0;
    uint dirtyPadding1;
    uint dirtyPadding2;
    ShadowDirtyPage dirtyPages[];
};
layout(std430,binding=2) buffer ShadowAllocator
{
    uint nextPhysical;uint requested;uint rendered;uint cached;
    uint evicted;uint previousRequested;uint overflow;uint allocatorPadding;
};
layout(std430,binding=6) readonly buffer ShadowRasterCommands
{
    uint rasterTriangleCount;
    uint rasterOverflow;
    uint rasterPadding0;
    uint rasterPadding1;
    DrawArraysIndirectCommand rasterCommands[];
};
void main()
{
    uint index=gl_GlobalInvocationID.x;
    if(index>=dirtyPageCount)return;
    uint physical=dirtyPages[index].data.x;
    if(rasterOverflow!=0u)
    {
        atomicAnd(shadowPages[physical].state.z,~PAGE_QUEUED);
        return;
    }
    atomicAnd(shadowPages[physical].state.z,~(PAGE_DIRTY|PAGE_QUEUED));
    atomicAdd(rendered,1u);
}
)GLSL";

} // namespace Gpu
} // namespace Renderer

#define VIRTUAL_SHADOW_FINISH_COMPUTE VIRTUAL_SHADOW_FINISH_CACHE_COMPUTE

#endif
