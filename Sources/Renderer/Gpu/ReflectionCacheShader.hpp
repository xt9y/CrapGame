#ifndef CRAPGAME_RENDERER_GPU_REFLECTIONCACHESHADER_HPP
#define CRAPGAME_RENDERER_GPU_REFLECTIONCACHESHADER_HPP

namespace Renderer
{
namespace Gpu
{

constexpr const char *REFLECTION_CACHE_GLSL=R"GLSL(
layout(binding=11) uniform sampler2D sReflectionPreviousPosition;
layout(binding=12) uniform sampler2D sReflectionPreviousNormalRoughness;
layout(binding=13) uniform usampler2D sReflectionPreviousMaterial;
layout(binding=14) uniform usampler2D sMaterialIdentity;
uniform mat4 uReflectionPreviousViewProjection;
uniform int uReflectionHistoryValid;

bool reflectionScreenHit(vec3 origin,vec3 direction,out vec3 source){
    ivec2 dimensions=textureSize(sPositionDepth,0);
    for(int stepIndex=1;stepIndex<=12;++stepIndex){
        float distanceValue=float(stepIndex)*0.45;
        vec3 samplePosition=origin+direction*distanceValue;
        vec4 clip=uViewProjection*vec4(samplePosition,1.0);
        if(clip.w<=EPSILON)continue;
        vec2 uv=clip.xy/clip.w*0.5+0.5;
        if(uv.x<=0.0||uv.x>=1.0||uv.y<=0.0||uv.y>=1.0)continue;
        ivec2 samplePixel=clamp(ivec2(uv*vec2(dimensions)),ivec2(0),dimensions-ivec2(1));
        vec4 surface=texelFetch(sPositionDepth,samplePixel,0);
        if(surface.w<=0.0)continue;
        float tolerance=max(0.08,0.04*distanceValue);
        if(distance(surface.xyz,samplePosition)<=tolerance){
            source=texelFetch(sDirect,samplePixel,0).xyz;
            return true;
        }
    }
    return false;
}

bool reflectionHistorySample(vec3 position,vec3 normal,float roughness,uint materialId,out vec3 reflectionValue){
    if(uReflectionHistoryValid==0)return false;
    vec4 clip=uReflectionPreviousViewProjection*vec4(position,1.0);
    if(clip.w<=EPSILON)return false;
    vec2 uv=clip.xy/clip.w*0.5+0.5;
    if(uv.x<0.0||uv.x>1.0||uv.y<0.0||uv.y>1.0)return false;
    ivec2 dimensions=textureSize(sReflectionPreviousPosition,0);
    ivec2 historyPixel=clamp(ivec2(uv*vec2(dimensions)),ivec2(0),dimensions-ivec2(1));
    vec4 previousPosition=texelFetch(sReflectionPreviousPosition,historyPixel,0);
    if(previousPosition.w<=0.0)return false;
    if(texelFetch(sReflectionPreviousMaterial,historyPixel,0).r!=materialId)return false;
    float tolerance=max(0.03,0.01*length(position-uCameraPosition));
    if(distance(previousPosition.xyz,position)>tolerance)return false;
    vec4 previousNormalRoughness=texelFetch(sReflectionPreviousNormalRoughness,historyPixel,0);
    if(dot(normalize(previousNormalRoughness.xyz),normalize(normal))<0.96)return false;
    if(abs(previousNormalRoughness.w-roughness)>0.05)return false;
    reflectionValue=texelFetch(sPreviousReflection,historyPixel,0).xyz;
    return true;
}

bool reflectionFallbackSource(vec3 position,vec3 normal,float roughness,uint materialId,
                              vec3 origin,vec3 direction,
                              out vec3 source,out bool historyValue){
    historyValue=false;
    if(reflectionScreenHit(origin,direction,source))return true;
    if(roughness>=0.35&&radianceCacheLookup(position,normal,source))return true;
    if(roughness<0.35&&reflectionHistorySample(position,normal,roughness,materialId,source)){
        historyValue=true;
        return true;
    }
    int hitIndex;float hitDistance;vec3 hitNormal;
    if(traceScene(origin,direction,48.0,hitIndex,hitDistance,hitNormal)){
        vec3 hitPosition=origin+direction*hitDistance;
        source=screenRadiance(hitPosition,hitIndex);
        return true;
    }
    return false;
}
)GLSL";

} // namespace Gpu
} // namespace Renderer

#endif
