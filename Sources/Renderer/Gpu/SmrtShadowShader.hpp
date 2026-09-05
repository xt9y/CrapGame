#ifndef CRAPGAME_RENDERER_GPU_SMRTSHADOWSHADER_HPP
#define CRAPGAME_RENDERER_GPU_SMRTSHADOWSHADER_HPP

#include "Renderer/Gpu/GBufferReconstruct.hpp"

#include <string>

namespace Renderer
{
namespace Gpu
{

constexpr const char *SMRT_SHADOW_GLSL=R"GLSL(
#define SMRT_PAGE_SIZE 128
#define SMRT_LEVEL0_PAGES 128
#define SMRT_MAX_PHYSICAL_PAGES 2048u
#define SMRT_ATLAS_PAGES_X 64u
#define SMRT_ATLAS_PAGES_Y 32u
#define SMRT_ATLAS_WIDTH 8192.0
#define SMRT_ATLAS_HEIGHT 4096.0
#define SMRT_FIRST_CLIPMAP_LEVEL 6
#define SMRT_LAST_CLIPMAP_LEVEL 22
#define SMRT_PAGE_VALID 1u
#define SMRT_PAGE_DIRTY 2u
#define SMRT_INVALID_PAGE 0xffffffffu
const int DIRECTIONAL_RAYS=7;
const int DIRECTIONAL_SAMPLES_PER_RAY=8;
const int LOCAL_RAYS=7;
const int LOCAL_SAMPLES_PER_RAY=8;
const float DIRECTIONAL_RAY_LENGTH_SCALE=1.5;
const float DIRECTIONAL_EXTRAPOLATE_MAX_SLOPE=5.0;
const float LOCAL_EXTRAPOLATE_MAX_SLOPE=0.05;
const float SCREEN_RAY_LENGTH=0.015;
const float NORMAL_BIAS=0.5;

struct SmrtShadowPhysicalPage
{
    ivec4 key;
    uvec4 state;
};
struct SmrtShadowClipmapData
{
    mat4 viewProjection;
    vec4 originExtent;
    ivec4 pageOffsetLevel;
    vec4 parameters;
};
struct SmrtShadowLightData
{
    vec4 sourceShape;
};
layout(std430,binding=0) readonly buffer SmrtShadowPageMetadata
{
    SmrtShadowPhysicalPage smrtShadowPages[];
};
layout(std430,binding=1) readonly buffer SmrtShadowPageTable
{
    uint smrtShadowPageTable[];
};
layout(std430,binding=2) readonly buffer SmrtShadowClipmapBuffer
{
    SmrtShadowClipmapData smrtShadowClipmaps[];
};
layout(std430,binding=3) readonly buffer SmrtShadowLightBuffer
{
    SmrtShadowLightData smrtShadowLights[];
};
layout(binding=0) uniform sampler2D sSmrtDepth;
layout(binding=1) uniform sampler2D sSmrtNormalRoughness;
layout(binding=2) uniform sampler2D sSmrtShadowAtlas;
uniform mat4 uSmrtInverseViewProjection;
uniform mat4 uSmrtViewProjection;
uniform vec3 uSmrtCameraPosition;
uniform vec3 uSmrtLightDirection;
uniform int uSmrtLightIndex;
uniform int uSmrtLightType;
uniform int uSmrtShadowClipmapCount;
uniform uint uSmrtFrameIndex;

vec3 gbufferReconstructWorld(
    ivec2 pixel,ivec2 dimensions,float depthValue,mat4 inverseViewProjection);

uint smrtHash(uint value)
{
    value^=value>>16u;
    value*=0x7feb352du;
    value^=value>>15u;
    value*=0x846ca68bu;
    value^=value>>16u;
    return value;
}

float smrtRandom(uint value)
{
    return float(smrtHash(value)&0x00ffffffu)/float(0x01000000u);
}

vec2 sampleDisk(int rayIndex,int rayCount,ivec2 pixel,uint frameIndex)
{
    const float GOLDEN_ANGLE=2.39996322972865332;
    uint seed=uint(pixel.x)*1973u^uint(pixel.y)*9277u^frameIndex*26699u;
    float rotation=smrtRandom(seed)*6.28318530717958648;
    float radius=sqrt((float(rayIndex)+0.5)/float(max(rayCount,1)));
    float angle=float(rayIndex)*GOLDEN_ANGLE+rotation;
    return vec2(cos(angle),sin(angle))*radius;
}

int smrtWrapPage(int value)
{
    int result=value%SMRT_LEVEL0_PAGES;
    return result<0?result+SMRT_LEVEL0_PAGES:result;
}

int smrtSelectClipmap(vec3 worldPosition)
{
    float cameraDistance=max(length(worldPosition-uSmrtCameraPosition),1.0);
    int level=SMRT_FIRST_CLIPMAP_LEVEL+int(floor(log2(cameraDistance)));
    level=clamp(level,SMRT_FIRST_CLIPMAP_LEVEL,SMRT_LAST_CLIPMAP_LEVEL);
    return clamp(level-SMRT_FIRST_CLIPMAP_LEVEL,0,max(uSmrtShadowClipmapCount-1,0));
}

bool smrtPageMatches(
    uint physical,int lightIndex,SmrtShadowClipmapData clipmap,ivec2 worldPage)
{
    if(physical>=SMRT_MAX_PHYSICAL_PAGES)return false;
    SmrtShadowPhysicalPage page=smrtShadowPages[physical];
    if((page.state.z&SMRT_PAGE_VALID)==0u||(page.state.z&SMRT_PAGE_DIRTY)!=0u)
        return false;
    return page.key.x==lightIndex&&
           page.key.y==clipmap.pageOffsetLevel.z&&
           page.key.z==worldPage.x&&page.key.w==worldPage.y;
}

bool sampleVirtualShadowPage(
    vec3 worldPosition,
    int lightIndex,
    int firstClipmap,
    out float storedDepth,
    out float receiverDepth,
    out float texelWorldSize,
    out int resolvedClipmap)
{
    storedDepth=1.0;
    receiverDepth=0.0;
    texelWorldSize=1.0;
    resolvedClipmap=-1;

    for(int fallback=0;fallback<8;++fallback)
    {
        int clipmapIndex=firstClipmap+fallback;
        if(clipmapIndex<0||clipmapIndex>=uSmrtShadowClipmapCount)break;
        SmrtShadowClipmapData clipmap=smrtShadowClipmaps[clipmapIndex];
        vec4 clip=clipmap.viewProjection*vec4(worldPosition,1.0);
        if(abs(clip.w)<=1e-8)continue;
        vec3 ndc=clip.xyz/clip.w;
        vec2 uv=ndc.xy*0.5+0.5;
        if(any(lessThan(uv,vec2(0.0)))||any(greaterThanEqual(uv,vec2(1.0))))
            continue;

        vec2 virtualTexel=uv*float(SMRT_LEVEL0_PAGES*SMRT_PAGE_SIZE);
        ivec2 localPage=clamp(
            ivec2(floor(virtualTexel/float(SMRT_PAGE_SIZE))),
            ivec2(0),ivec2(SMRT_LEVEL0_PAGES-1));
        ivec2 worldPage=clipmap.pageOffsetLevel.xy+
            localPage-ivec2(SMRT_LEVEL0_PAGES/2);
        int wrappedX=smrtWrapPage(worldPage.x);
        int wrappedY=smrtWrapPage(worldPage.y);
        uint tableSlot=uint(
            clipmapIndex*SMRT_LEVEL0_PAGES*SMRT_LEVEL0_PAGES+
            wrappedY*SMRT_LEVEL0_PAGES+wrappedX);
        uint physical=smrtShadowPageTable[tableSlot];
        if(!smrtPageMatches(physical,lightIndex,clipmap,worldPage))continue;

        vec2 pageUv=fract(virtualTexel/float(SMRT_PAGE_SIZE));
        uvec2 atlasPage=uvec2(
            physical%SMRT_ATLAS_PAGES_X,
            physical/SMRT_ATLAS_PAGES_X);
        vec2 atlasTexel=vec2(atlasPage)*float(SMRT_PAGE_SIZE)+
            pageUv*float(SMRT_PAGE_SIZE)+vec2(0.5);
        vec2 atlasUv=atlasTexel/vec2(SMRT_ATLAS_WIDTH,SMRT_ATLAS_HEIGHT);

        storedDepth=texture(sSmrtShadowAtlas,atlasUv).r;
        receiverDepth=ndc.z*0.5+0.5;
        texelWorldSize=max(clipmap.parameters.x,1e-6);
        resolvedClipmap=clipmapIndex;
        return true;
    }

    return false;
}

vec3 smrtBasisTangent(vec3 direction)
{
    vec3 reference=abs(direction.y)>0.95?vec3(1.0,0.0,0.0):vec3(0.0,1.0,0.0);
    return normalize(cross(reference,direction));
}

vec3 smrtSourceDirection(
    vec3 towardLight,
    vec2 disk,
    int lightType,
    float maximumDistance,
    float sourceRadius,
    float sourceAngle)
{
    vec3 forward=normalize(towardLight);
    vec3 tangent=smrtBasisTangent(forward);
    vec3 bitangent=normalize(cross(forward,tangent));
    float angularRadius=lightType==0
        ? max(sourceAngle,0.0)
        : atan(max(sourceRadius,0.0)/max(maximumDistance,1e-4));
    float spread=tan(min(angularRadius,1.45));
    return normalize(forward+(tangent*disk.x+bitangent*disk.y)*spread);
}

vec3 smrtSmartReceiverStart(
    ivec2 pixel,
    vec3 position,
    vec3 normal,
    vec3 towardLight,
    int clipmapIndex)
{
    float ignoredStored,ignoredReceiver,texelWorldSize;
    int ignoredClipmap;
    if(!sampleVirtualShadowPage(
        position,uSmrtLightIndex,clipmapIndex,
        ignoredStored,ignoredReceiver,texelWorldSize,ignoredClipmap))
    {
        texelWorldSize=max(
            smrtShadowClipmaps[clipmapIndex].parameters.x,
            1e-6);
    }

    vec3 start=position+normalize(normal)*texelWorldSize*NORMAL_BIAS;
    float screenDistance=max(
        texelWorldSize*2.0,
        length(start-uSmrtCameraPosition)*SCREEN_RAY_LENGTH);
    vec3 endPoint=start+normalize(towardLight)*screenDistance;
    vec4 endClip=uSmrtViewProjection*vec4(endPoint,1.0);
    if(abs(endClip.w)<=1e-8)return start;
    vec2 endUv=endClip.xy/endClip.w*0.5+0.5;
    ivec2 dimensions=textureSize(sSmrtDepth,0);
    vec2 startUv=(vec2(pixel)+vec2(0.5))/vec2(dimensions);

    for(int step=1;step<=3;++step)
    {
        float amount=float(step)/3.0;
        vec2 uv=mix(startUv,endUv,amount);
        if(any(lessThan(uv,vec2(0.0)))||any(greaterThanEqual(uv,vec2(1.0))))
            break;
        ivec2 samplePixel=clamp(
            ivec2(uv*vec2(dimensions)),ivec2(0),dimensions-ivec2(1));
        float depth=texelFetch(sSmrtDepth,samplePixel,0).r;
        if(depth>=0.999999)continue;
        vec3 screenWorld=gbufferReconstructWorld(
            samplePixel,dimensions,depth,uSmrtInverseViewProjection);
        vec3 delta=screenWorld-start;
        float along=dot(delta,normalize(towardLight));
        vec3 lateral=delta-normalize(towardLight)*along;
        if(along>texelWorldSize*0.25&&along<screenDistance&&
           length(lateral)<texelWorldSize*2.0)
        {
            start=screenWorld+normalize(normal)*texelWorldSize*NORMAL_BIAS;
        }
    }

    return start;
}

float smrtDepthBias(float texelWorldSize,float slopeLimit,vec3 normal,vec3 rayDirection)
{
    float grazing=1.0-clamp(abs(dot(normalize(normal),normalize(rayDirection))),0.0,1.0);
    return max(1e-5,texelWorldSize*(0.00035+0.00035*grazing*min(slopeLimit,5.0)));
}

float smrtTraceRay(
    vec3 origin,
    vec3 normal,
    vec3 rayDirection,
    int lightIndex,
    int lightType,
    int clipmapIndex,
    float maximumDistance,
    int sampleCount,
    out float blockerDistance)
{
    blockerDistance=maximumDistance;
    float slopeLimit=lightType==0
        ? DIRECTIONAL_EXTRAPOLATE_MAX_SLOPE
        : LOCAL_EXTRAPOLATE_MAX_SLOPE;
    float previousStored=1.0,
          previousReceiver=0.0,
          previousDistance=0.0;
    bool previousValid=false;

    for(int sampleIndex=0;sampleIndex<LOCAL_SAMPLES_PER_RAY;++sampleIndex)
    {
        if(sampleIndex>=sampleCount)break;
        float amount=(float(sampleIndex)+0.5)/float(max(sampleCount,1));
        float distanceAlong=maximumDistance*amount;
        vec3 samplePosition=origin+rayDirection*distanceAlong;
        float storedDepth,receiverDepth,texelWorldSize;
        int resolvedClipmap;
        if(!sampleVirtualShadowPage(
            samplePosition,lightIndex,clipmapIndex,
            storedDepth,receiverDepth,texelWorldSize,resolvedClipmap))
        {
            continue;
        }

        float extrapolatedStored=storedDepth;
        if(previousValid)
        {
            float receiverDelta=receiverDepth-previousReceiver;
            float slope=abs(receiverDelta)>1e-6
                ? (storedDepth-previousStored)/receiverDelta
                : 0.0;
            slope=clamp(slope,-slopeLimit,slopeLimit);
            extrapolatedStored=previousStored+slope*receiverDelta;
        }

        float bias=smrtDepthBias(texelWorldSize,slopeLimit,normal,rayDirection);
        if(receiverDepth>min(storedDepth,extrapolatedStored)+bias)
        {
            blockerDistance=min(blockerDistance,distanceAlong);
            return 0.0;
        }

        previousStored=storedDepth;
        previousReceiver=receiverDepth;
        previousDistance=distanceAlong;
        previousValid=true;
    }

    return 1.0;
}

float virtualShadowVisibility(
    vec3 position,
    vec3 normal,
    vec3 lightDirection,
    int lightIndex,
    int lightType,
    float maximumDistance,
    float sourceRadius,
    float sourceAngle,
    uint frameIndex)
{
    vec3 towardLight=normalize(lightDirection);
    int clipmapIndex=smrtSelectClipmap(position);
    ivec2 dimensions=textureSize(sSmrtDepth,0);
    vec4 receiverClip=uSmrtViewProjection*vec4(position,1.0);
    vec2 receiverUv=abs(receiverClip.w)>1e-8
        ? receiverClip.xy/receiverClip.w*0.5+0.5
        : vec2(0.5);
    ivec2 pixel=clamp(
        ivec2(receiverUv*vec2(dimensions)),ivec2(0),dimensions-ivec2(1));
    vec3 origin=smrtSmartReceiverStart(
        pixel,position,normal,towardLight,clipmapIndex);

    int rayCount=lightType==0?DIRECTIONAL_RAYS:LOCAL_RAYS;
    int sampleCount=lightType==0
        ? DIRECTIONAL_SAMPLES_PER_RAY
        : LOCAL_SAMPLES_PER_RAY;
    float rayLength=maximumDistance;
    if(lightType==0)
    {
        rayLength=max(
            smrtShadowClipmaps[clipmapIndex].originExtent.w*
                DIRECTIONAL_RAY_LENGTH_SCALE,
            smrtShadowClipmaps[clipmapIndex].parameters.y*2.0);
    }

    float visibilitySum=0.0,
          blockerDistance=rayLength,
          blockerDistanceSum=0.0;
    int blockedRays=0;
    int evaluatedRays=0;
    const int ADAPTIVE_RAY_COUNT=3;

    for(int rayIndex=0;rayIndex<LOCAL_RAYS;++rayIndex)
    {
        if(rayIndex>=rayCount)break;
        vec2 disk=sampleDisk(rayIndex,rayCount,pixel,frameIndex);
        vec3 rayDirection=smrtSourceDirection(
            towardLight,disk,lightType,rayLength,sourceRadius,sourceAngle);
        float rayBlockerDistance;
        float rayVisibility=smrtTraceRay(
            origin,normal,rayDirection,lightIndex,lightType,clipmapIndex,
            rayLength,sampleCount,rayBlockerDistance);
        visibilitySum+=rayVisibility;
        ++evaluatedRays;
        if(rayVisibility<0.5)
        {
            ++blockedRays;
            blockerDistance=min(blockerDistance,rayBlockerDistance);
            blockerDistanceSum+=rayBlockerDistance;
        }

        if(rayIndex+1==ADAPTIVE_RAY_COUNT)
        {
            if(blockedRays==0)return 1.0;
            if(blockedRays==ADAPTIVE_RAY_COUNT)return 0.0;
        }
    }

    float visibility=visibilitySum/float(max(evaluatedRays,1));
    if(blockedRays>0)
    {
        float averageBlockerDistance=blockerDistanceSum/float(blockedRays);
        float contact=clamp(
            averageBlockerDistance/max(rayLength,1e-4),0.0,1.0);
        visibility=mix(visibility,visibility*visibility,1.0-contact);
    }
    return clamp(visibility,0.0,1.0);
}
)GLSL";

constexpr const char *SMRT_SHADOW_COMPUTE_HEADER=R"GLSL(
#version 430 core
layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
layout(r16f,binding=0) writeonly uniform image2D oSmrtVisibility;
)GLSL";

constexpr const char *SMRT_SHADOW_COMPUTE_MAIN=R"GLSL(
void main()
{
    ivec2 pixel=ivec2(gl_GlobalInvocationID.xy);
    ivec2 dimensions=textureSize(sSmrtDepth,0);
    if(any(greaterThanEqual(pixel,dimensions)))return;
    float depth=texelFetch(sSmrtDepth,pixel,0).r;
    if(depth>=0.999999||uSmrtLightIndex<0)
    {
        imageStore(oSmrtVisibility,pixel,vec4(1.0));
        return;
    }

    vec3 position=gbufferReconstructWorld(
        pixel,dimensions,depth,uSmrtInverseViewProjection);
    vec3 normal=normalize(texelFetch(sSmrtNormalRoughness,pixel,0).xyz);
    SmrtShadowLightData shadowLight=smrtShadowLights[uSmrtLightIndex];
    float maximumDistance=100000.0;
    float visibility=virtualShadowVisibility(
        position,normal,normalize(-uSmrtLightDirection),
        uSmrtLightIndex,uSmrtLightType,maximumDistance,
        shadowLight.sourceShape.x,shadowLight.sourceShape.y,uSmrtFrameIndex);
    imageStore(oSmrtVisibility,pixel,vec4(visibility));
}
)GLSL";

inline std::string smrtShadowComputeShader ()
{
    return std::string(SMRT_SHADOW_COMPUTE_HEADER)+
        SMRT_SHADOW_GLSL+
        GBUFFER_RECONSTRUCT_GLSL+
        SMRT_SHADOW_COMPUTE_MAIN;
}

} // namespace Gpu
} // namespace Renderer

#endif
