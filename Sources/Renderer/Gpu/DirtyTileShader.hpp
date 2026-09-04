#ifndef CRAPGAME_RENDERER_GPU_DIRTYTILESHADER_HPP
#define CRAPGAME_RENDERER_GPU_DIRTYTILESHADER_HPP

namespace Renderer
{
namespace Gpu
{

constexpr const char *DIRTY_TILE_COMPACT_COMPUTE = R"GLSL(
#version 430 core
layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
layout(binding=0) uniform usampler2D sValidMask;
layout(std430,binding=0) writeonly buffer DirtyTileBuffer { uvec2 dirtyTiles[]; };
layout(std430,binding=1) buffer DispatchBuffer {
    uint groupCountX;
    uint groupCountY;
    uint groupCountZ;
};
shared uint tileDirty;
void main()
{
    uint localIndex=gl_LocalInvocationIndex;
    if(localIndex==0u) tileDirty=0u;
    barrier();

    ivec2 pixel=ivec2(gl_GlobalInvocationID.xy);
    ivec2 dimensions=textureSize(sValidMask,0);
    if(pixel.x<dimensions.x&&pixel.y<dimensions.y
            &&texelFetch(sValidMask,pixel,0).r==0u)
        atomicOr(tileDirty,1u);
    barrier();

    if(localIndex==0u&&tileDirty!=0u)
    {
        uint index=atomicAdd(groupCountX,1u);
        dirtyTiles[index]=gl_WorkGroupID.xy;
    }
}
)GLSL";

} // namespace Gpu
} // namespace Renderer

#endif
