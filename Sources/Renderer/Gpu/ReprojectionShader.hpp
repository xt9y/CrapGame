#ifndef CRAPGAME_RENDERER_GPU_REPROJECTIONSHADER_HPP
#define CRAPGAME_RENDERER_GPU_REPROJECTIONSHADER_HPP

namespace Renderer
{
namespace Gpu
{

constexpr const char *REPROJECTION_COMPUTE = R"GLSL(
#version 430 core
layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
layout(binding=0) uniform sampler2D sCurrentDepth;
layout(binding=1) uniform sampler2D sCurrentNormalRoughness;
layout(binding=2) uniform usampler2D sCurrentMaterial;
layout(binding=3) uniform sampler2D sPreviousPosition;
layout(binding=4) uniform sampler2D sPreviousNormal;
layout(binding=5) uniform usampler2D sPreviousMaterial;
layout(binding=6) uniform sampler2D sPreviousIndirect;
layout(binding=7) uniform sampler2D sPreviousReflection;
layout(rgba16f,binding=0) writeonly uniform image2D oIndirect;
layout(rgba16f,binding=1) writeonly uniform image2D oReflection;
layout(r8ui,binding=2) writeonly uniform uimage2D oValid;
layout(rgba16f,binding=3) writeonly uniform image2D oPositionHistory;
uniform mat4 uPreviousViewProjection;
uniform mat4 uCurrentInverseViewProjection;
uniform vec3 uCameraPosition;
uniform int uHistoryValid;
bool depthValid(float depthValue){return depthValue<0.999999;}
vec3 reconstructWorld(ivec2 pixel,ivec2 dimensions,float depthValue){vec2 uv=(vec2(pixel)+vec2(0.5))/vec2(dimensions);vec4 world=uCurrentInverseViewProjection*vec4(uv*2.0-1.0,depthValue*2.0-1.0,1.0);return abs(world.w)>1e-8?world.xyz/world.w:world.xyz;}
void reject(ivec2 pixel){imageStore(oIndirect,pixel,vec4(0.0));imageStore(oReflection,pixel,vec4(0.0));imageStore(oValid,pixel,uvec4(0u));}
void main(){
    ivec2 tracePixel=ivec2(gl_GlobalInvocationID.xy),traceDimensions=imageSize(oIndirect);
    if(tracePixel.x>=traceDimensions.x||tracePixel.y>=traceDimensions.y)return;
    if(uHistoryValid==0){reject(tracePixel);imageStore(oPositionHistory,tracePixel,vec4(0.0));return;}
    ivec2 fullDimensions=textureSize(sCurrentDepth,0);
    ivec2 fullPixel=min(tracePixel*2+ivec2(1),fullDimensions-ivec2(1));
    float currentDepth=texelFetch(sCurrentDepth,fullPixel,0).r;
    if(!depthValid(currentDepth)){reject(tracePixel);imageStore(oPositionHistory,tracePixel,vec4(0.0));return;}
    vec3 currentWorld=reconstructWorld(fullPixel,fullDimensions,currentDepth);
    vec4 currentPosition=vec4(currentWorld,1.0);
    imageStore(oPositionHistory,tracePixel,currentPosition);
    vec3 currentNormal=normalize(texelFetch(sCurrentNormalRoughness,fullPixel,0).xyz);
    uint currentMaterial=texelFetch(sCurrentMaterial,fullPixel,0).r;
    vec4 clip=uPreviousViewProjection*vec4(currentPosition.xyz,1.0);
    if(clip.w<=0.00001){reject(tracePixel);return;}
    vec2 uv=clip.xy/clip.w*0.5+0.5;
    if(uv.x<0.0||uv.x>1.0||uv.y<0.0||uv.y>1.0){reject(tracePixel);return;}
    ivec2 historyDimensions=textureSize(sPreviousPosition,0);
    ivec2 historyPixel=clamp(ivec2(uv*vec2(historyDimensions)),ivec2(0),historyDimensions-ivec2(1));
    vec4 previousPosition=texelFetch(sPreviousPosition,historyPixel,0);
    if(previousPosition.w<=0.0){reject(tracePixel);return;}
    uint previousMaterial=texelFetch(sPreviousMaterial,historyPixel,0).r;
    if(currentMaterial!=previousMaterial){reject(tracePixel);return;}
    float cameraDistance=length(currentPosition.xyz-uCameraPosition);
    float tolerance=max(0.03,0.01*cameraDistance);
    if(distance(previousPosition.xyz,currentPosition.xyz)>tolerance){reject(tracePixel);return;}
    vec3 previousNormal=normalize(texelFetch(sPreviousNormal,historyPixel,0).xyz);
    float normalDot=dot(previousNormal,currentNormal);
    if(normalDot < 0.94){reject(tracePixel);return;}
    imageStore(oIndirect,tracePixel,texelFetch(sPreviousIndirect,historyPixel,0));
    imageStore(oReflection,tracePixel,texelFetch(sPreviousReflection,historyPixel,0));
    imageStore(oValid,tracePixel,uvec4(1u,0u,0u,0u));
}
)GLSL";

constexpr const char *REPROJECTION_CAPTURE_COMPUTE = R"GLSL(
#version 430 core
layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
layout(binding=0) uniform sampler2D sCurrentDepth;
layout(binding=1) uniform sampler2D sCurrentNormalRoughness;
layout(binding=2) uniform usampler2D sCurrentMaterial;
layout(rgba16f,binding=0) writeonly uniform image2D oPreviousPosition;
layout(rgba16f,binding=1) writeonly uniform image2D oPreviousNormal;
layout(r32ui,binding=2) writeonly uniform uimage2D oPreviousMaterial;
uniform mat4 uCurrentInverseViewProjection;
bool depthValid(float depthValue){return depthValue<0.999999;}
vec3 reconstructWorld(ivec2 pixel,ivec2 dimensions,float depthValue){vec2 uv=(vec2(pixel)+vec2(0.5))/vec2(dimensions);vec4 world=uCurrentInverseViewProjection*vec4(uv*2.0-1.0,depthValue*2.0-1.0,1.0);return abs(world.w)>1e-8?world.xyz/world.w:world.xyz;}
void main(){
    ivec2 tracePixel=ivec2(gl_GlobalInvocationID.xy),traceDimensions=imageSize(oPreviousPosition);
    if(tracePixel.x>=traceDimensions.x||tracePixel.y>=traceDimensions.y)return;
    ivec2 fullDimensions=textureSize(sCurrentDepth,0);
    ivec2 fullPixel=min(tracePixel*2+ivec2(1),fullDimensions-ivec2(1));
    float depth=texelFetch(sCurrentDepth,fullPixel,0).r;
    vec4 position=depthValid(depth)?vec4(reconstructWorld(fullPixel,fullDimensions,depth),1.0):vec4(0.0);
    vec4 normal=texelFetch(sCurrentNormalRoughness,fullPixel,0);
    uint material=texelFetch(sCurrentMaterial,fullPixel,0).r;
    imageStore(oPreviousPosition,tracePixel,position);
    imageStore(oPreviousNormal,tracePixel,normal);
    imageStore(oPreviousMaterial,tracePixel,uvec4(material,0u,0u,0u));
}
)GLSL";

} // namespace Gpu
} // namespace Renderer

#endif
