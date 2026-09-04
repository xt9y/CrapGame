#ifndef CRAPGAME_RENDERER_GPU_DIRECTLIGHTINGIMPORTEDSHADER_HPP
#define CRAPGAME_RENDERER_GPU_DIRECTLIGHTINGIMPORTEDSHADER_HPP

#include "Renderer/Gpu/DirectLightingShader.hpp"
#include "Renderer/Gpu/StaticShadowShader.hpp"
#include "Renderer/Gpu/TriangleTraceShader.hpp"

#include <stdexcept>
#include <string>

namespace Renderer { namespace Gpu {

inline void replaceDirectRequired(std::string *source,
                                  const std::string& from,
                                  const std::string& to)
{
    const std::size_t at=source?source->find(from):std::string::npos;
    if(at==std::string::npos)
        throw std::runtime_error("direct-light shader patch point missing");
    source->replace(at,from.size(),to);
}

inline std::string directLightingImportedShader()
{
    std::string source = DIRECT_LIGHTING_COMPUTE;
    const std::string insertion = "float pointAttenuation";
    const std::size_t at = source.find(insertion);
    if (at == std::string::npos)
        throw std::runtime_error("direct-light shader insertion point missing");
    source.insert(at, std::string(IMPORTED_TRIANGLE_TRACE_GLSL)
                    + STATIC_SHADOW_DIRECT_GLSL);

    const std::string old_shadow =
        "if(light.coneShadow.z>0.5&&shadowed(position,normal,ld,maxD))continue;";
    const std::string new_shadow =
        "if(dot(normal,ld)<=0.0)continue;"
        "if(light.coneShadow.z>0.5){"
        "if(shadowed(position,normal,ld,maxD))continue;"
        "float importedVisibility=(uStaticShadowEnabled!=0&&i==uStaticShadowLightIndex)"
        "?staticShadowVisibility(position,normal,ld)"
        ":importedShadowVisibility(position+normal*SHADOW_BIAS*2.0,ld,maxD);"
        "if(importedVisibility<=0.0)continue;"
        "radiance*=importedVisibility;}";
    const std::size_t shadow_at = source.find(old_shadow);
    if (shadow_at == std::string::npos)
        throw std::runtime_error("direct-light shadow call missing");
    source.replace(shadow_at, old_shadow.size(), new_shadow);

    const std::string old_material =
        "vec3 position=pd.xyz,normal=normalize(nr.xyz),albedo=am.xyz,viewDirection=normalize(uCameraPosition-position),emissive=eo.xyz;float roughness=nr.w,metallic=am.w;vec3 direct=emissive;";
    const std::string dynamic_material =
        "vec3 position=pd.xyz,normal=normalize(nr.xyz),albedo=am.xyz,viewDirection=normalize(uCameraPosition-position);float roughness=nr.w,metallic=am.w;vec3 direct=vec3(0.0);";
    replaceDirectRequired(&source,old_material,dynamic_material);
    replaceDirectRequired(&source,",eo=imageLoad(gEmissive,pixel)","");
    replaceDirectRequired(&source,
        "uniform int uPrimitiveCount;",
        "uniform int uPrimitiveCount;uniform int uStaticSplitLightIndex;");
    replaceDirectRequired(&source,
        "if(pd.w<=0){imageStore(oDirect,pixel,vec4(0.055,0.070,0.105,1));return;}",
        "if(pd.w<=0){imageStore(oDirect,pixel,vec4(0.0));return;}");
    replaceDirectRequired(&source,
        "for(int i=0;i<uLightCount;++i){LightData light=lights[i];",
        "for(int i=0;i<uLightCount;++i){if(i==uStaticSplitLightIndex)continue;LightData light=lights[i];");
    return source;
}

} }
#endif
