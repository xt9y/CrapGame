#ifndef CRAPGAME_RENDERER_GPU_LUMENIMPORTEDSTAGEASHADER_HPP
#define CRAPGAME_RENDERER_GPU_LUMENIMPORTEDSTAGEASHADER_HPP

#include "Renderer/Gpu/GBufferReconstruct.hpp"
#include "Renderer/Gpu/LumenImportedShader.hpp"

#include <stdexcept>
#include <string>

namespace Renderer
{
namespace Gpu
{

inline void replaceStageARequired(std::string *source,
                                  const std::string& from,
                                  const std::string& to)
{
    const std::size_t at=source?source->find(from):std::string::npos;
    if(at==std::string::npos)
        throw std::runtime_error("Lumen Stage-A GBuffer patch point missing");
    source->replace(at,from.size(),to);
}

inline std::string lumenImportedStageATraceShader()
{
    std::string source=lumenImportedTraceShader();

    const std::size_t helper_at=source.find("const float EPSILON=0.00001;");
    if(helper_at==std::string::npos)
        throw std::runtime_error("Lumen Stage-A helper insertion point missing");
    source.insert(helper_at,GBUFFER_RECONSTRUCT_GLSL);

    replaceStageARequired(&source,
        "vec4 sp=texelFetch(sPositionDepth,samplePixel,0);if(sp.w>0&&distance(sp.xyz,position)<0.35)return texelFetch(sDirect,samplePixel,0).xyz;",
        "float sampleDepth=texelFetch(sPositionDepth,samplePixel,0).r;if(gbufferDepthValid(sampleDepth)){vec3 samplePosition=gbufferReconstructWorld(samplePixel,dimensions,sampleDepth,inverse(uViewProjection));if(distance(samplePosition,position)<0.35)return texelFetch(sDirect,samplePixel,0).xyz;}");

    replaceStageARequired(&source,
        "ivec2 fd=textureSize(sPositionDepth,0),pixel=min(tp*2+ivec2(1),fd-ivec2(1));vec4 pd=texelFetch(sPositionDepth,pixel,0);if(pd.w<=0){imageStore(oIndirect,tp,vec4(0));imageStore(oReflection,tp,vec4(0));imageStore(oPositionHistory,tp,vec4(0));return;}",
        "ivec2 fd=textureSize(sPositionDepth,0),pixel=min(tp*2+ivec2(1),fd-ivec2(1));float currentDepth=texelFetch(sPositionDepth,pixel,0).r;if(!gbufferDepthValid(currentDepth)){imageStore(oIndirect,tp,vec4(0));imageStore(oReflection,tp,vec4(0));imageStore(oPositionHistory,tp,vec4(0));return;}vec4 pd=vec4(gbufferReconstructWorld(pixel,fd,currentDepth,inverse(uViewProjection)),1.0);");

    replaceStageARequired(&source,
        "vec4 surface=texelFetch(sPositionDepth,samplePixel,0);\n        if(surface.w<=0.0)continue;\n        float tolerance=max(0.08,0.04*distanceValue);\n        if(distance(surface.xyz,samplePosition)<=tolerance){",
        "float surfaceDepth=texelFetch(sPositionDepth,samplePixel,0).r;\n        if(!gbufferDepthValid(surfaceDepth))continue;\n        vec3 surface=gbufferReconstructWorld(samplePixel,dimensions,surfaceDepth,inverse(uViewProjection));\n        float tolerance=max(0.08,0.04*distanceValue);\n        if(distance(surface,samplePosition)<=tolerance){");

    replaceStageARequired(&source,
        "vec3 lumenEnvironmentRadiance(vec3 direction){\n    float up=clamp(direction.y*0.5+0.5,0.0,1.0);\n    float horizon=pow(clamp(1.0-abs(direction.y),0.0,1.0),2.0);\n    vec3 sky=mix(vec3(0.025,0.030,0.040),vec3(0.16,0.21,0.30),up);\n    return sky+vec3(0.030,0.024,0.016)*horizon;\n}",
        "vec3 lumenGroundRadiance(){return vec3(0.045,0.042,0.038);}\n"
        "vec3 lumenEnvironmentRadiance(vec3 direction){\n"
        "    vec3 d=normalize(direction);\n"
        "    float y=clamp(d.y,-1.0,1.0);\n"
        "    vec3 horizon=vec3(0.18,0.19,0.21);\n"
        "    vec3 zenith=vec3(0.24,0.30,0.40);\n"
        "    if(y>=0.0)return mix(horizon,zenith,pow(y,0.55));\n"
        "    return mix(horizon,lumenGroundRadiance(),pow(-y,0.65));\n"
        "}\n"
        "vec3 lumenEnvironmentIrradiance(vec3 normal){\n"
        "    float up=clamp(normalize(normal).y*0.5+0.5,0.0,1.0);\n"
        "    vec3 lower=mix(lumenGroundRadiance(),vec3(0.085,0.088,0.092),up);\n"
        "    vec3 upper=mix(vec3(0.085,0.088,0.092),vec3(0.120,0.145,0.185),up);\n"
        "    return mix(lower,upper,up)*0.42;\n"
        "}");

    replaceStageARequired(&source,
        "vec3 n=normalize(normal),outgoing=max(emissive,vec3(0.0));",
        "vec3 n=normalize(normal),outgoing=max(emissive,vec3(0.0));outgoing+=base*(1.0-metallic)*lumenEnvironmentIrradiance(n);");

    /* A readable cache value is useful as a stable estimate, but it must not
     * stop stochastic refinement. Continue tracing/updating until the cache's
     * high-confidence cap while shading from the already-converged estimate. */
    replaceStageARequired(&source,
        "giSource=vec3(0.0),indirect=vec3(0.0);bool giCached=radianceCacheLookup(position,normal,giSource);if(!giCached){",
        "cachedGiSource=vec3(0.0),giSource=vec3(0.0),indirect=vec3(0.0);bool giCached=radianceCacheLookup(position,normal,cachedGiSource);{");
    replaceStageARequired(&source,
        "radianceCacheUpdate(position,normal,cacheSource);}indirect=giSource*albedo*(1.0-metallic);",
        "radianceCacheUpdate(position,normal,cacheSource);if(giCached)giSource=cachedGiSource;}indirect=giSource*albedo*(1.0-metallic);");

    /* Static scenes receive dozens of refinement samples. Preserve more of the
     * validated history so Monte-Carlo variance decays instead of freezing as
     * visible low-frequency blotches. */
    replaceStageARequired(&source,
        "indirect=mix(indirect,texelFetch(sPreviousIndirect,bestHistoryPixel,0).xyz,0.86);",
        "indirect=mix(indirect,texelFetch(sPreviousIndirect,bestHistoryPixel,0).xyz,0.94);");

    replaceStageARequired(&source,
        "uniform int uDirtyTileDispatch;",
        "uniform int uDirtyTileDispatch;\nuniform int uTraceSliceIndex;\nuniform int uTraceSliceCount;");
    replaceStageARequired(&source,
        "void main(){ivec2 td=imageSize(oIndirect);ivec2 tp=uDirtyTileDispatch!=0?ivec2(dirtyTiles[gl_WorkGroupID.x]*8u+gl_LocalInvocationID.xy):ivec2(gl_GlobalInvocationID.xy);",
        "void main(){ivec2 td=imageSize(oIndirect);uint traceSliceCount=uint(max(uTraceSliceCount,1));uint logicalGroupY=gl_WorkGroupID.y*traceSliceCount+uint(max(uTraceSliceIndex,0));ivec2 slicedTp=ivec2(int(gl_WorkGroupID.x*8u+gl_LocalInvocationID.x),int(logicalGroupY*8u+gl_LocalInvocationID.y));ivec2 tp=uDirtyTileDispatch!=0?ivec2(dirtyTiles[gl_WorkGroupID.x]*8u+gl_LocalInvocationID.xy):(uTraceSliceCount>1?slicedTp:ivec2(gl_GlobalInvocationID.xy));");

    return source;
}

} // namespace Gpu
} // namespace Renderer

#endif
