#ifndef CRAPGAME_RENDERER_GPU_DIRECTLIGHTINGIMPORTEDSHADER_HPP
#define CRAPGAME_RENDERER_GPU_DIRECTLIGHTINGIMPORTEDSHADER_HPP

#include "Renderer/Gpu/DirectLightingShader.hpp"
#include "Renderer/Gpu/TriangleTraceShader.hpp"

#include <stdexcept>
#include <string>

namespace Renderer { namespace Gpu {

inline std::string directLightingImportedShader()
{
    std::string source = DIRECT_LIGHTING_COMPUTE;
    const std::string insertion = "float pointAttenuation";
    const std::size_t at = source.find(insertion);
    if (at == std::string::npos)
        throw std::runtime_error("direct-light shader insertion point missing");
    source.insert(at, IMPORTED_TRIANGLE_TRACE_GLSL);

    const std::string old_shadow =
        "if(light.coneShadow.z>0.5&&shadowed(position,normal,ld,maxD))continue;";
    const std::string new_shadow =
        "if(light.coneShadow.z>0.5){"
        "if(shadowed(position,normal,ld,maxD))continue;"
        "float importedVisibility=importedShadowVisibility(position+normal*SHADOW_BIAS*2.0,ld,maxD);"
        "if(importedVisibility<=0.0)continue;"
        "radiance*=importedVisibility;}";
    const std::size_t shadow_at = source.find(old_shadow);
    if (shadow_at == std::string::npos)
        throw std::runtime_error("direct-light shadow call missing");
    source.replace(shadow_at, old_shadow.size(), new_shadow);

    const std::string old_material =
        "vec3 position=pd.xyz,normal=normalize(nr.xyz),albedo=am.xyz,viewDirection=normalize(uCameraPosition-position),emissive=eo.xyz;float roughness=nr.w,metallic=am.w;vec3 direct=emissive;";
    const std::string new_material =
        "vec3 position=pd.xyz,normal=normalize(nr.xyz),albedo=am.xyz,viewDirection=normalize(uCameraPosition-position),emissive=eo.xyz,ambient=texelFetch(sAmbientTransmission,pixel,0).rgb;float roughness=nr.w,metallic=am.w;vec3 direct=emissive+ambient*albedo*0.025;";
    const std::size_t material_at = source.find(old_material);
    if (material_at == std::string::npos)
        throw std::runtime_error("direct-light ambient material patch point missing");
    source.replace(material_at, old_material.size(), new_material);
    return source;
}

} }
#endif
