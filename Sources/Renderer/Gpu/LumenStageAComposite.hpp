#ifndef CRAPGAME_RENDERER_GPU_LUMENSTAGEACOMPOSITE_HPP
#define CRAPGAME_RENDERER_GPU_LUMENSTAGEACOMPOSITE_HPP

namespace Renderer
{
namespace Gpu
{

constexpr const char *LUMEN_STAGE_A_COMPOSITE_COMPUTE=R"GLSL(
#version 430 core
layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
layout(binding=0) uniform sampler2D sPositionDepth;
layout(binding=1) uniform sampler2D sNormalRoughness;
layout(binding=2) uniform sampler2D sDirect;
layout(binding=3) uniform sampler2D sIndirect;
layout(binding=4) uniform sampler2D sReflection;
layout(rgba8,binding=0) writeonly uniform image2D oFinal;
uniform mat4 uGBufferInverseViewProjection;

const float OUTPUT_EXPOSURE=1.15;
bool gbufferDepthValid(float depthValue){return depthValue<0.999999;}
vec3 gbufferReconstructWorld(ivec2 pixel,ivec2 dimensions,float depthValue){
    vec2 uv=(vec2(pixel)+vec2(0.5))/vec2(dimensions);
    vec4 world=uGBufferInverseViewProjection*vec4(uv*2.0-1.0,depthValue*2.0-1.0,1.0);
    return abs(world.w)>1e-8?world.xyz/world.w:world.xyz;
}
vec3 acesToneMap(vec3 colorValue){
    vec3 x=max(colorValue,vec3(0.0))*OUTPUT_EXPOSURE;
    const float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14;
    return clamp((x*(a*x+b))/(x*(c*x+d)+e),vec3(0.0),vec3(1.0));
}
vec3 toneMap(vec3 colorValue){return pow(acesToneMap(colorValue),vec3(1.0/2.2));}
vec3 bilateralIndirect(ivec2 pixel,vec3 position,vec3 normal){
    ivec2 fullDimensions=textureSize(sPositionDepth,0);
    ivec2 halfDimensions=textureSize(sIndirect,0);
    ivec2 centerHalf=clamp(pixel/2,ivec2(0),halfDimensions-ivec2(1));
    const ivec2 offsets[9]=ivec2[9](
        ivec2(0,0),
        ivec2(1,0),ivec2(-1,0),ivec2(0,1),ivec2(0,-1),
        ivec2(1,1),ivec2(-1,1),ivec2(1,-1),ivec2(-1,-1)
    );
    vec3 sum=vec3(0.0);
    float totalWeight=0.0;
    for(int index=0;index<9;++index){
        ivec2 sampleHalf=clamp(centerHalf+offsets[index],ivec2(0),halfDimensions-ivec2(1));
        ivec2 samplePixel=clamp(sampleHalf*2+ivec2(1),ivec2(0),fullDimensions-ivec2(1));
        float sampleDepth=texelFetch(sPositionDepth,samplePixel,0).r;
        if(!gbufferDepthValid(sampleDepth))continue;
        vec3 samplePosition=gbufferReconstructWorld(samplePixel,fullDimensions,sampleDepth);
        vec3 sampleNormal=normalize(texelFetch(sNormalRoughness,samplePixel,0).xyz);
        float normalWeight=pow(max(dot(normal,sampleNormal),0.0),12.0);
        float positionWeight=exp(-distance(position,samplePosition)*5.0);
        vec2 offsetVector=vec2(offsets[index]);
        float spatialWeight=exp(-0.55*dot(offsetVector,offsetVector));
        float weight=spatialWeight*normalWeight*positionWeight;
        if(weight<=0.0001)continue;
        sum+=texelFetch(sIndirect,sampleHalf,0).xyz*weight;
        totalWeight+=weight;
    }
    if(totalWeight<=0.0001)return texelFetch(sIndirect,centerHalf,0).xyz;
    return sum/totalWeight;
}
float shortRangeAo(ivec2 pixel,vec3 position,vec3 normal){
    ivec2 dimensions=textureSize(sPositionDepth,0);
    const ivec2 offsets[8]=ivec2[8](ivec2(2,0),ivec2(-2,0),ivec2(0,2),ivec2(0,-2),ivec2(2,2),ivec2(-2,2),ivec2(2,-2),ivec2(-2,-2));
    float occlusion=0.0;
    for(int index=0;index<8;++index){
        ivec2 samplePixel=clamp(pixel+offsets[index],ivec2(0),dimensions-ivec2(1));
        float sampleDepth=texelFetch(sPositionDepth,samplePixel,0).r;
        if(!gbufferDepthValid(sampleDepth))continue;
        vec3 samplePosition=gbufferReconstructWorld(samplePixel,dimensions,sampleDepth);
        vec3 delta=samplePosition-position;float distanceValue=length(delta);
        if(distanceValue<=0.01||distanceValue>=0.65)continue;
        float facing=max(dot(normalize(delta),normal)-0.10,0.0);
        occlusion+=facing*(1.0-distanceValue/0.65);
    }
    return clamp(1.0-occlusion*0.12,0.70,1.0);
}
void main(){
    ivec2 pixel=ivec2(gl_GlobalInvocationID.xy),dimensions=textureSize(sPositionDepth,0);
    if(pixel.x>=dimensions.x||pixel.y>=dimensions.y)return;
    float depth=texelFetch(sPositionDepth,pixel,0).r;
    vec3 direct=texelFetch(sDirect,pixel,0).xyz;
    if(!gbufferDepthValid(depth)){imageStore(oFinal,pixel,vec4(toneMap(direct),1.0));return;}
    vec3 position=gbufferReconstructWorld(pixel,dimensions,depth);
    vec3 normal=normalize(texelFetch(sNormalRoughness,pixel,0).xyz);
    ivec2 halfDimensions=textureSize(sIndirect,0);
    ivec2 halfPixel=clamp(pixel/2,ivec2(0),halfDimensions-ivec2(1));
    vec3 indirect=bilateralIndirect(pixel,position,normal);
    vec3 reflection=texelFetch(sReflection,halfPixel,0).xyz;
    float ao=shortRangeAo(pixel,position,normal);
    imageStore(oFinal,pixel,vec4(toneMap(direct+indirect*ao+reflection),1.0));
}
)GLSL";

} // namespace Gpu
} // namespace Renderer

#endif
