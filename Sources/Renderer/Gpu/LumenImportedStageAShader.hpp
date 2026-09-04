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

    return source;
}

} // namespace Gpu
} // namespace Renderer

#endif
