#ifndef CRAPGAME_RENDERER_GPU_VIRTUALSHADOWMAPSHADER_HPP
#define CRAPGAME_RENDERER_GPU_VIRTUALSHADOWMAPSHADER_HPP

#include "Renderer/Gpu/GBufferReconstruct.hpp"

#include <string>

namespace Renderer
{
namespace Gpu
{

constexpr const char *VIRTUAL_SHADOW_BEGIN_COMPUTE=R"GLSL(
#version 430 core
layout(local_size_x=1,local_size_y=1,local_size_z=1) in;
layout(std430,binding=2) buffer ShadowAllocator
{
    uint nextPhysical;
    uint requested;
    uint rendered;
    uint cached;
    uint evicted;
    uint previousRequested;
    uint overflow;
    uint padding;
};
struct ShadowDirtyPage{uvec4 data;};
layout(std430,binding=4) buffer ShadowDirtyPages
{
    uint dirtyPageCount;
    uint dirtyPadding0;
    uint dirtyPadding1;
    uint dirtyPadding2;
    ShadowDirtyPage dirtyPages[];
};
void main()
{
    previousRequested=requested;
    requested=0u;
    rendered=0u;
    cached=0u;
    evicted=0u;
    overflow=0u;
    dirtyPageCount=0u;
}
)GLSL";

constexpr const char *VIRTUAL_SHADOW_MARK_COMPUTE=R"GLSL(
#version 430 core
layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
#define PAGE_SIZE 128
#define LEVEL0_PAGES 128
#define RECEIVER_MASK_SIZE 8
#define MAX_PHYSICAL_PAGES 2048
#define FIRST_CLIPMAP_LEVEL 6
#define LAST_CLIPMAP_LEVEL 22
#define FIRST_COARSE_LEVEL 15
#define LAST_COARSE_LEVEL 18
#define PAGE_TABLE_LEVEL_SIZE (LEVEL0_PAGES*LEVEL0_PAGES)
#define INVALID_PAGE 0xffffffffu
#define LOCKED_PAGE 0xfffffffeu
#define PAGE_VALID 1u
layout(binding=0) uniform sampler2D sDepth;
uniform mat4 uGBufferInverseViewProjection;
uniform vec3 uCameraPosition;
uniform int uFrameIndex;
uniform int uDirectionalLightIndex;
uniform int uShadowClipmapCount;
struct ShadowPhysicalPage
{
    ivec4 key;
    uvec4 state;
};
struct ShadowClipmapData
{
    mat4 viewProjection;
    vec4 originExtent;
    ivec4 pageOffsetLevel;
    vec4 parameters;
};
layout(std430,binding=0) buffer ShadowPageMetadata
{
    ShadowPhysicalPage shadowPages[];
};
layout(std430,binding=1) buffer ShadowPageTable
{
    uint shadowPageTable[];
};
layout(std430,binding=2) buffer ShadowAllocator
{
    uint nextPhysical;
    uint requested;
    uint rendered;
    uint cached;
    uint evicted;
    uint previousRequested;
    uint overflow;
    uint allocatorPadding;
};
layout(std430,binding=3) readonly buffer ShadowClipmapBuffer
{
    ShadowClipmapData shadowClipmaps[];
};
struct ShadowDirtyPage{uvec4 data;};
layout(std430,binding=4) buffer ShadowDirtyPages
{
    uint dirtyPageCount;
    uint dirtyPadding0;
    uint dirtyPadding1;
    uint dirtyPadding2;
    ShadowDirtyPage dirtyPages[];
};
vec3 gbufferReconstructWorld(ivec2 pixel,ivec2 dimensions,float depthValue,mat4 inverseViewProjection);
int wrapShadowPage(int value)
{
    int result=value%LEVEL0_PAGES;
    return result<0?result+LEVEL0_PAGES:result;
}
float shadowDynamicLodBias()
{
    const float threshold=float(MAX_PHYSICAL_PAGES)*0.85;
    if(float(previousRequested)<=threshold)return 0.0;
    return clamp((float(previousRequested)-threshold)/max(1.0,float(MAX_PHYSICAL_PAGES)-threshold)*2.0,0.0,2.0);
}
bool shadowPageMatches(uint physical,ivec4 key,uint mip)
{
    if(physical>=MAX_PHYSICAL_PAGES)return false;
    ShadowPhysicalPage page=shadowPages[physical];
    return (page.state.z&PAGE_VALID)!=0u&&all(equal(page.key,key))&&page.state.w==mip;
}
uint chooseShadowPhysicalPage()
{
    uint frameIndex=uint(max(uFrameIndex,0));
    for(int attempt=0;attempt<32;++attempt)
    {
        uint candidate=atomicAdd(nextPhysical,1u)%MAX_PHYSICAL_PAGES;
        ShadowPhysicalPage page=shadowPages[candidate];
        if((page.state.z&PAGE_VALID)==0u||page.state.y!=frameIndex)return candidate;
    }
    return INVALID_PAGE;
}
bool requestDirectionalPage(vec3 worldPosition,int clipmapIndex,uint mip)
{
    if(clipmapIndex<0||clipmapIndex>=uShadowClipmapCount)return false;
    ShadowClipmapData clipmap=shadowClipmaps[clipmapIndex];
    vec4 clip=clipmap.viewProjection*vec4(worldPosition,1.0);
    if(abs(clip.w)<=1e-8)return false;
    vec2 uv=clip.xy/clip.w*0.5+0.5;
    if(any(lessThan(uv,vec2(0.0)))||any(greaterThanEqual(uv,vec2(1.0))))return false;
    ivec2 localPage=clamp(ivec2(floor(uv*float(LEVEL0_PAGES))),ivec2(0),ivec2(LEVEL0_PAGES-1));
    ivec2 worldPage=clipmap.pageOffsetLevel.xy+localPage-ivec2(LEVEL0_PAGES/2);
    int wrappedX=wrapShadowPage(worldPage.x),wrappedY=wrapShadowPage(worldPage.y);
    uint tableSlot=uint(clipmapIndex*PAGE_TABLE_LEVEL_SIZE+wrappedY*LEVEL0_PAGES+wrappedX);
    ivec4 key=ivec4(uDirectionalLightIndex,clipmap.pageOffsetLevel.z,worldPage.x,worldPage.y);
    uint frameIndex=uint(max(uFrameIndex,0));

    for(int retry=0;retry<4;++retry)
    {
        uint mapped=shadowPageTable[tableSlot];
        if(mapped==LOCKED_PAGE)continue;
        if(shadowPageMatches(mapped,key,mip))
        {
            atomicMax(shadowPages[mapped].state.y,frameIndex);
            atomicAdd(cached,1u);
            return true;
        }

        uint locked=atomicCompSwap(shadowPageTable[tableSlot],mapped,LOCKED_PAGE);
        if(locked!=mapped)continue;

        uint requestIndex=atomicAdd(requested,1u);
        if(requestIndex>=MAX_PHYSICAL_PAGES)
        {
            atomicAdd(overflow,1u);
            atomicExchange(shadowPageTable[tableSlot],mapped);
            return false;
        }

        uint physical=mapped;
        if(mapped==INVALID_PAGE||mapped>=MAX_PHYSICAL_PAGES)
        {
            physical=chooseShadowPhysicalPage();
            if(physical==INVALID_PAGE)
            {
                atomicAdd(overflow,1u);
                atomicExchange(shadowPageTable[tableSlot],INVALID_PAGE);
                return false;
            }
        }
        else
        {
            atomicAdd(evicted,1u);
        }

        ShadowPhysicalPage oldPage=shadowPages[physical];
        if((oldPage.state.z&PAGE_VALID)!=0u&&oldPage.state.x<uint(shadowPageTable.length()))
        {
            atomicCompSwap(shadowPageTable[oldPage.state.x],physical,INVALID_PAGE);
        }

        shadowPages[physical].key=key;
        shadowPages[physical].state=uvec4(tableSlot,frameIndex,PAGE_VALID|2u,mip);
        dirtyPages[requestIndex].data=uvec4(physical,uint(clipmapIndex),uvec2(localPage));
        atomicMax(dirtyPageCount,requestIndex+1u);
        memoryBarrierBuffer();
        atomicExchange(shadowPageTable[tableSlot],physical);
        return true;
    }

    atomicAdd(overflow,1u);
    return false;
}
int selectDirectionalClipmap(vec3 worldPosition)
{
    float distanceToCamera=max(length(worldPosition-uCameraPosition),1.0);
    int level=FIRST_CLIPMAP_LEVEL+int(floor(log2(distanceToCamera)));
    level+=int(floor(shadowDynamicLodBias()+0.5));
    level=clamp(level,FIRST_CLIPMAP_LEVEL,LAST_CLIPMAP_LEVEL);
    return level-FIRST_CLIPMAP_LEVEL;
}
void main()
{
    if(gl_LocalInvocationIndex!=0u)return;
    ivec2 dimensions=textureSize(sDepth,0);
    ivec2 receiverMask=ivec2(gl_WorkGroupID.xy);
    ivec2 pixel=receiverMask*RECEIVER_MASK_SIZE+ivec2(RECEIVER_MASK_SIZE/2);
    pixel=min(pixel,dimensions-ivec2(1));
    if(any(lessThan(pixel,ivec2(0))))return;
    float depth=texelFetch(sDepth,pixel,0).r;
    if(depth>=0.999999)return;
    vec3 worldPosition=gbufferReconstructWorld(pixel,dimensions,depth,uGBufferInverseViewProjection);
    int selected=selectDirectionalClipmap(worldPosition);
    requestDirectionalPage(worldPosition,selected,0u);
    for(int coarseLevel=FIRST_COARSE_LEVEL;coarseLevel<=LAST_COARSE_LEVEL;++coarseLevel)
    {
        int coarseIndex=coarseLevel-FIRST_CLIPMAP_LEVEL;
        if(coarseIndex!=selected)requestDirectionalPage(worldPosition,coarseIndex,0u);
    }
}
)GLSL";

inline std::string virtualShadowMarkShader ()
{
    return std::string(VIRTUAL_SHADOW_MARK_COMPUTE)+GBUFFER_RECONSTRUCT_GLSL;
}

} // namespace Gpu
} // namespace Renderer

#endif
