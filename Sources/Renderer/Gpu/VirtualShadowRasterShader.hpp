#ifndef CRAPGAME_RENDERER_GPU_VIRTUALSHADOWRASTERSHADER_HPP
#define CRAPGAME_RENDERER_GPU_VIRTUALSHADOWRASTERSHADER_HPP

namespace Renderer
{
namespace Gpu
{

constexpr const char *VIRTUAL_SHADOW_RASTER_BEGIN_COMPUTE=R"GLSL(
#version 430 core
layout(local_size_x=1,local_size_y=1,local_size_z=1) in;
struct DrawArraysIndirectCommand
{
    uint count;
    uint instanceCount;
    uint first;
    uint baseInstance;
};
layout(std430,binding=6) buffer ShadowRasterCommands
{
    uint rasterTriangleCount;
    uint rasterOverflow;
    uint rasterPadding0;
    uint rasterPadding1;
    DrawArraysIndirectCommand rasterCommands[];
};
void main()
{
    rasterTriangleCount=0u;
    rasterOverflow=0u;
}
)GLSL";

constexpr const char *VIRTUAL_SHADOW_PAGE_CULL_COMPUTE=R"GLSL(
#version 430 core
layout(local_size_x=1,local_size_y=1,local_size_z=1) in;
#define MAX_PHYSICAL_PAGES 2048u
#define MAX_RASTER_TRIANGLES 1048576u
#define LEVEL0_PAGES 128
struct ImportedMesh
{
    uint triangleOffset;
    uint triangleCount;
    uint nodeOffset;
    uint nodeCount;
};
struct ImportedInstance
{
    mat4 inverseModel;
    uint meshIndex;
    uint materialHandle;
    uint flags;
    uint padding;
};
struct TraceBvhNode
{
    vec4 boundsMinimum;
    vec4 boundsMaximum;
    ivec4 meta;
};
struct ShadowDirtyPage
{
    uvec4 data;
};
struct ShadowClipmapData
{
    mat4 viewProjection;
    vec4 originExtent;
    ivec4 pageOffsetLevel;
    vec4 parameters;
};
struct ShadowRasterTriangleRef
{
    uvec4 data;
};
struct DrawArraysIndirectCommand
{
    uint count;
    uint instanceCount;
    uint first;
    uint baseInstance;
};
layout(std430,binding=0) readonly buffer ImportedMeshBuffer
{
    ImportedMesh importedMeshes[];
};
layout(std430,binding=1) readonly buffer ImportedInstanceBuffer
{
    ImportedInstance importedInstances[];
};
layout(std430,binding=2) readonly buffer ImportedTlasBuffer
{
    TraceBvhNode importedTlasNodes[];
};
layout(std430,binding=3) readonly buffer ShadowDirtyPages
{
    uint dirtyPageCount;
    uint dirtyPadding0;
    uint dirtyPadding1;
    uint dirtyPadding2;
    ShadowDirtyPage dirtyPages[];
};
layout(std430,binding=4) readonly buffer ShadowClipmapBuffer
{
    ShadowClipmapData shadowClipmaps[];
};
layout(std430,binding=5) buffer ShadowRasterTriangles
{
    ShadowRasterTriangleRef rasterTriangles[];
};
layout(std430,binding=6) buffer ShadowRasterCommands
{
    uint rasterTriangleCount;
    uint rasterOverflow;
    uint rasterPadding0;
    uint rasterPadding1;
    DrawArraysIndirectCommand rasterCommands[];
};
uniform int uImportedInstanceCount;
uniform int uImportedTlasNodeCount;
uniform int uShadowClipmapCount;

bool pageIntersectsNode(ShadowDirtyPage page,TraceBvhNode node)
{
    int clipmapIndex=int(page.data.y);
    if(clipmapIndex<0||clipmapIndex>=uShadowClipmapCount)return false;
    ShadowClipmapData clipmap=shadowClipmaps[clipmapIndex];
    vec2 pageMinimum=vec2(page.data.zw)/float(LEVEL0_PAGES);
    vec2 pageMaximum=vec2(page.data.zw+uvec2(1u))/float(LEVEL0_PAGES);
    vec2 minimumUv=vec2(1e30),maximumUv=vec2(-1e30);
    float minimumDepth=1e30,maximumDepth=-1e30;
    for(int corner=0;corner<8;++corner)
    {
        vec3 p=vec3(
            (corner&1)!=0?node.boundsMaximum.x:node.boundsMinimum.x,
            (corner&2)!=0?node.boundsMaximum.y:node.boundsMinimum.y,
            (corner&4)!=0?node.boundsMaximum.z:node.boundsMinimum.z);
        vec4 clip=clipmap.viewProjection*vec4(p,1.0);
        if(abs(clip.w)<=1e-8)return true;
        vec3 ndc=clip.xyz/clip.w;
        vec2 uv=ndc.xy*0.5+0.5;
        minimumUv=min(minimumUv,uv);
        maximumUv=max(maximumUv,uv);
        minimumDepth=min(minimumDepth,ndc.z);
        maximumDepth=max(maximumDepth,ndc.z);
    }
    bool xy=all(greaterThanEqual(maximumUv,pageMinimum))&&
            all(lessThanEqual(minimumUv,pageMaximum));
    bool z=maximumDepth>=-1.0&&minimumDepth<=1.0;
    return xy&&z;
}

uint packShadowPage(uint physical,uint clipmapIndex,uint localX,uint localY)
{
    return (physical&0x7ffu)|((clipmapIndex&0x1fu)<<11u)|
           ((localX&0x7fu)<<16u)|((localY&0x7fu)<<23u);
}

uint countPageTriangles(ShadowDirtyPage page)
{
    if(uImportedTlasNodeCount<=0||uImportedInstanceCount<=0)return 0u;
    uint triangleCount=0u;
    int stack[64];int stackSize=1;stack[0]=0;
    while(stackSize>0)
    {
        int nodeIndex=stack[--stackSize];
        if(nodeIndex<0||nodeIndex>=uImportedTlasNodeCount)continue;
        TraceBvhNode node=importedTlasNodes[nodeIndex];
        if(!pageIntersectsNode(page,node))continue;
        if(node.meta.w>0)
        {
            for(int j=0;j<node.meta.w;++j)
            {
                int instanceIndex=j==0?node.meta.x:(j==1?node.meta.y:node.meta.z);
                if(instanceIndex<0||instanceIndex>=uImportedInstanceCount)continue;
                ImportedInstance instance=importedInstances[instanceIndex];
                if((instance.flags&2u)!=0u)continue;
                triangleCount+=importedMeshes[instance.meshIndex].triangleCount;
            }
        }
        else if(stackSize<=61)
        {
            stack[stackSize++]=node.meta.x;
            stack[stackSize++]=node.meta.y;
        }
    }
    return triangleCount;
}

void writePageTriangles(ShadowDirtyPage page,uint destination)
{
    int stack[64];int stackSize=1;stack[0]=0;
    uint cursor=destination;
    uint packed=packShadowPage(page.data.x,page.data.y,page.data.z,page.data.w);
    while(stackSize>0)
    {
        int nodeIndex=stack[--stackSize];
        if(nodeIndex<0||nodeIndex>=uImportedTlasNodeCount)continue;
        TraceBvhNode node=importedTlasNodes[nodeIndex];
        if(!pageIntersectsNode(page,node))continue;
        if(node.meta.w>0)
        {
            for(int j=0;j<node.meta.w;++j)
            {
                int instanceIndex=j==0?node.meta.x:(j==1?node.meta.y:node.meta.z);
                if(instanceIndex<0||instanceIndex>=uImportedInstanceCount)continue;
                ImportedInstance instance=importedInstances[instanceIndex];
                if((instance.flags&2u)!=0u)continue;
                ImportedMesh mesh=importedMeshes[instance.meshIndex];
                for(uint localTriangle=0u;localTriangle<mesh.triangleCount;++localTriangle)
                {
                    rasterTriangles[cursor++].data=uvec4(
                        mesh.triangleOffset+localTriangle,
                        uint(instanceIndex),packed,0u);
                }
            }
        }
        else if(stackSize<=61)
        {
            stack[stackSize++]=node.meta.x;
            stack[stackSize++]=node.meta.y;
        }
    }
}

void main()
{
    uint pageIndex=gl_WorkGroupID.x;
    if(pageIndex>=MAX_PHYSICAL_PAGES)return;
    rasterCommands[pageIndex]=DrawArraysIndirectCommand(0u,0u,0u,0u);
    if(pageIndex>=dirtyPageCount)return;
    ShadowDirtyPage page=dirtyPages[pageIndex];
    uint pageTriangles=countPageTriangles(page);
    if(pageTriangles==0u)return;
    uint firstTriangle=atomicAdd(rasterTriangleCount,pageTriangles);
    if(firstTriangle>=MAX_RASTER_TRIANGLES||pageTriangles>MAX_RASTER_TRIANGLES-firstTriangle)
    {
        atomicAdd(rasterOverflow,1u);
        return;
    }
    writePageTriangles(page,firstTriangle);
    rasterCommands[pageIndex]=DrawArraysIndirectCommand(
        pageTriangles*3u,1u,firstTriangle*3u,0u);
}
)GLSL";

constexpr const char *VIRTUAL_SHADOW_CLEAR_VERTEX=R"GLSL(
#version 430 core
struct ShadowDirtyPage{uvec4 data;};
layout(std430,binding=0) readonly buffer ShadowDirtyPages
{
    uint dirtyPageCount;
    uint dirtyPadding0;
    uint dirtyPadding1;
    uint dirtyPadding2;
    ShadowDirtyPage dirtyPages[];
};
const vec2 QUAD[6]=vec2[6](
    vec2(0,0),vec2(1,0),vec2(1,1),
    vec2(0,0),vec2(1,1),vec2(0,1));
void main()
{
    uint index=uint(gl_InstanceID);
    if(index>=dirtyPageCount){gl_Position=vec4(2,2,1,1);return;}
    uint physical=dirtyPages[index].data.x;
    uvec2 page=uvec2(physical%64u,physical/64u);
    vec2 atlasUv=(vec2(page)*128.0+QUAD[gl_VertexID]*128.0)/vec2(8192.0,4096.0);
    gl_Position=vec4(atlasUv*2.0-1.0,1.0,1.0);
}
)GLSL";

constexpr const char *VIRTUAL_SHADOW_CLEAR_FRAGMENT=R"GLSL(
#version 430 core
void main(){}
)GLSL";

constexpr const char *VIRTUAL_SHADOW_RASTER_VERTEX=R"GLSL(
#version 430 core
struct ShadowRasterTriangleRef{uvec4 data;};
struct ImportedTriangle
{
    vec4 p0;vec4 p1;vec4 p2;
    vec4 uv0Uv1;vec4 uv2Material;
    vec4 n0;vec4 n1;vec4 n2;
};
struct ImportedInstance
{
    mat4 inverseModel;
    uint meshIndex;
    uint materialHandle;
    uint flags;
    uint padding;
};
struct ShadowClipmapData
{
    mat4 viewProjection;
    vec4 originExtent;
    ivec4 pageOffsetLevel;
    vec4 parameters;
};
layout(std430,binding=0) readonly buffer ShadowRasterTriangles{ShadowRasterTriangleRef rasterTriangles[];};
layout(std430,binding=1) readonly buffer ImportedTriangleBuffer{ImportedTriangle importedTriangles[];};
layout(std430,binding=2) readonly buffer ImportedInstanceBuffer{ImportedInstance importedInstances[];};
layout(std430,binding=3) readonly buffer ShadowClipmapBuffer{ShadowClipmapData shadowClipmaps[];};
out vec2 vUv;
flat out uint vPhysicalPage;
flat out uint vMaterialHandle;

void decodeShadowPage(uint packed,out uint physical,out uint clipmapIndex,out uvec2 localPage)
{
    physical=packed&0x7ffu;
    clipmapIndex=(packed>>11u)&0x1fu;
    localPage=uvec2((packed>>16u)&0x7fu,(packed>>23u)&0x7fu);
}

void main()
{
    uint referenceIndex=uint(gl_VertexID)/3u;
    uint corner=uint(gl_VertexID)%3u;
    ShadowRasterTriangleRef reference=rasterTriangles[referenceIndex];
    ImportedTriangle triangle=importedTriangles[reference.data.x];
    ImportedInstance instance=importedInstances[reference.data.y];
    vec3 localPosition=corner==0u?triangle.p0.xyz:(corner==1u?triangle.p1.xyz:triangle.p2.xyz);
    vec2 uv=corner==0u?triangle.uv0Uv1.xy:(corner==1u?triangle.uv0Uv1.zw:triangle.uv2Material.xy);
    uint physical,clipmapIndex;uvec2 localPage;
    decodeShadowPage(reference.data.z,physical,clipmapIndex,localPage);
    mat4 model=inverse(instance.inverseModel);
    vec4 world=model*vec4(localPosition,1.0);
    vec4 clip=shadowClipmaps[clipmapIndex].viewProjection*world;
    vec2 virtualUv=clip.xy/clip.w*0.5+0.5;
    vec2 pageUv=virtualUv*128.0-vec2(localPage);
    uvec2 atlasPage=uvec2(physical%64u,physical/64u);
    vec2 atlasUv=(vec2(atlasPage)*128.0+pageUv*128.0)/vec2(8192.0,4096.0);
    gl_Position=vec4((atlasUv*2.0-1.0)*clip.w,clip.z,clip.w);
    vUv=uv;
    vPhysicalPage=physical;
    vMaterialHandle=instance.materialHandle;
}
)GLSL";

constexpr const char *VIRTUAL_SHADOW_RASTER_FRAGMENT=R"GLSL(
#version 430 core
in vec2 vUv;
flat in uint vPhysicalPage;
flat in uint vMaterialHandle;
struct TraceRecord
{
    vec4 baseMetallic;
    vec4 emissiveRoughness;
    vec4 specularIor;
    vec4 advanced;
    vec4 transmission;
    vec4 extra;
    ivec4 textureIndices[4];
};
layout(std430,binding=4) readonly buffer TraceRecordBuffer{TraceRecord traceRecords[];};
layout(binding=0) uniform sampler2DArray sTraceColorAtlas;
layout(binding=1) uniform sampler2DArray sTraceDataAtlas;
uniform int uTraceMaterialCount;
const int TRACE_SLOT_OPACITY=6;
int traceTextureIndex(TraceRecord material,int slot)
{
    ivec4 q=material.textureIndices[slot>>2];int c=slot&3;
    return c==0?q.x:(c==1?q.y:(c==2?q.z:q.w));
}
float traceChannel(vec4 value,int channel)
{
    if(channel==1)return value.g;if(channel==2)return value.b;
    if(channel==3)return value.a;if(channel==4)return dot(value.rgb,vec3(1.0/3.0));
    return value.r;
}
vec4 traceTexture(int descriptorIndex,vec2 uv)
{
    if(descriptorIndex<0)return vec4(1.0);
    TraceRecord d=traceRecords[descriptorIndex];
    vec2 tuv=uv*d.emissiveRoughness.xy+d.emissiveRoughness.zw;
    ivec4 meta=d.textureIndices[0];
    tuv=meta.w!=0?clamp(tuv,vec2(0),vec2(1)):fract(tuv);
    vec2 atlasUv=d.baseMetallic.xy+tuv*d.baseMetallic.zw;
    return meta.y==0?texture(sTraceColorAtlas,vec3(atlasUv,float(meta.x))):
                     texture(sTraceDataAtlas,vec3(atlasUv,float(meta.x)));
}
float traceScalar(TraceRecord material,int slot,vec2 uv,float fallbackValue)
{
    int descriptor=traceTextureIndex(material,slot);
    if(descriptor<0)return fallbackValue;
    TraceRecord d=traceRecords[descriptor];
    return traceChannel(traceTexture(descriptor,uv),d.textureIndices[0].z);
}
float traceResolvedOpacity(int materialHandle,vec2 uv)
{
    if(materialHandle<0||materialHandle>=uTraceMaterialCount)return 1.0;
    TraceRecord material=traceRecords[materialHandle];
    return clamp(material.advanced.x*traceScalar(material,TRACE_SLOT_OPACITY,uv,1.0),0.0,1.0);
}
void main()
{
    uvec2 page=uvec2(vPhysicalPage%64u,vPhysicalPage/64u);
    vec2 minimum=vec2(page)*128.0;
    vec2 maximum=minimum+vec2(128.0);
    if(gl_FragCoord.x<minimum.x||gl_FragCoord.y<minimum.y||
       gl_FragCoord.x>=maximum.x||gl_FragCoord.y>=maximum.y)discard;
    int material=int(vMaterialHandle);
    if(material<0||material>=uTraceMaterialCount)return;
    int renderClass=int(traceRecords[material].extra.w+0.5);
    if(renderClass==2)discard;
    if(renderClass==1&&traceResolvedOpacity(material,vUv)<traceRecords[material].advanced.w)discard;
}
)GLSL";

constexpr const char *VIRTUAL_SHADOW_FINISH_COMPUTE=R"GLSL(
#version 430 core
layout(local_size_x=64,local_size_y=1,local_size_z=1) in;
#define PAGE_DIRTY 2u
struct ShadowPhysicalPage{ivec4 key;uvec4 state;};
struct ShadowDirtyPage{uvec4 data;};
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
void main()
{
    uint index=gl_GlobalInvocationID.x;
    if(index>=dirtyPageCount)return;
    uint physical=dirtyPages[index].data.x;
    atomicAnd(shadowPages[physical].state.z,~PAGE_DIRTY);
    atomicAdd(rendered,1u);
}
)GLSL";

constexpr const char *VIRTUAL_SHADOW_LOOKUP_GLSL=R"GLSL(
#define SHADOW_INVALID_PAGE 0xffffffffu
#define SHADOW_LEVEL0_PAGES 128
#define SHADOW_FIRST_CLIPMAP_LEVEL 6
#define SHADOW_MAX_MIP_LEVELS 8
uint shadowResolvePhysicalPage(vec3 worldPosition,int clipmapIndex,uint mip)
{
    uint parentMip=mip;
    for(;parentMip<uint(SHADOW_MAX_MIP_LEVELS);++parentMip)
    {
        int candidate=clipmapIndex+int(parentMip-mip);
        if(candidate>=uShadowClipmapCount)break;
        ShadowClipmapData clipmap=shadowClipmaps[candidate];
        vec4 clip=clipmap.viewProjection*vec4(worldPosition,1.0);
        if(abs(clip.w)<=1e-8)continue;
        vec2 uv=clip.xy/clip.w*0.5+0.5;
        if(any(lessThan(uv,vec2(0)))||any(greaterThanEqual(uv,vec2(1))))continue;
        ivec2 localPage=clamp(ivec2(floor(uv*float(SHADOW_LEVEL0_PAGES))),ivec2(0),ivec2(SHADOW_LEVEL0_PAGES-1));
        ivec2 worldPage=clipmap.pageOffsetLevel.xy+localPage-ivec2(SHADOW_LEVEL0_PAGES/2);
        int wrappedX=((worldPage.x%SHADOW_LEVEL0_PAGES)+SHADOW_LEVEL0_PAGES)%SHADOW_LEVEL0_PAGES;
        int wrappedY=((worldPage.y%SHADOW_LEVEL0_PAGES)+SHADOW_LEVEL0_PAGES)%SHADOW_LEVEL0_PAGES;
        uint slot=uint(candidate*SHADOW_LEVEL0_PAGES*SHADOW_LEVEL0_PAGES+
                       wrappedY*SHADOW_LEVEL0_PAGES+wrappedX);
        uint physical=shadowPageTable[slot];
        if(physical<2048u&&(shadowPages[physical].state.z&1u)!=0u)return physical;
    }
    return SHADOW_INVALID_PAGE;
}
)GLSL";

} // namespace Gpu
} // namespace Renderer

#endif
