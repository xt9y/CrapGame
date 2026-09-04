#ifndef CRAPGAME_RENDERER_GPU_LUMENIMPORTEDSHADER_HPP
#define CRAPGAME_RENDERER_GPU_LUMENIMPORTEDSHADER_HPP

#include "Renderer/Gpu/BvhShadersV2.hpp"
#include "Renderer/Gpu/TriangleTraceShader.hpp"

#include <stdexcept>
#include <string>

namespace Renderer { namespace Gpu {

inline void replaceLumenRequired(std::string *source,
                                 const std::string& from,
                                 const std::string& to)
{
    const std::size_t at = source ? source->find(from) : std::string::npos;
    if (at == std::string::npos)
        throw std::runtime_error("Lumen imported shader patch point missing");
    source->replace(at, from.size(), to);
}

inline std::string lumenImportedTraceShader()
{
    std::string source = LUMEN_TRACE_BVH_V2_COMPUTE;
    replaceLumenRequired(&source,
        "layout(std430,binding=7) readonly buffer PrimitiveBuffer",
        "layout(std430,binding=5) readonly buffer PrimitiveBuffer");

    std::string imported = IMPORTED_TRIANGLE_TRACE_GLSL;
    replaceLumenRequired(&imported,
        "layout(binding=4) uniform sampler2DArray sTraceColorAtlas;",
        "layout(binding=7) uniform sampler2DArray sTraceColorAtlas;");
    replaceLumenRequired(&imported,
        "layout(binding=5) uniform sampler2DArray sTraceDataAtlas;",
        "layout(binding=8) uniform sampler2DArray sTraceDataAtlas;");

    const std::size_t rotate_at = source.find("vec3 rotateX");
    if (rotate_at == std::string::npos)
        throw std::runtime_error("Lumen imported shader insertion point missing");
    source.insert(rotate_at, imported);

    replaceLumenRequired(&source, "bool traceScene(", "bool traceAnalyticScene(");

    const char *combined = R"GLSL(
int gImportedMaterial=-1;
vec2 gImportedUv=vec2(0.0);
vec3 gImportedNormal=vec3(0.0);
bool gImportedHit=false;

vec3 importedMappedNormal(int materialHandle,vec2 uv,vec3 geometricNormal){
    if(materialHandle<0||materialHandle>=uTraceMaterialCount)return geometricNormal;
    TraceRecord material=traceRecords[materialHandle];
    int descriptor=traceTextureIndex(material,7);
    if(descriptor<0)return geometricNormal;
    vec3 tangentNormal=normalize(traceTexture(descriptor,uv).xyz*2.0-1.0);
    vec3 n=normalize(geometricNormal);
    vec3 helper=abs(n.y)<0.999?vec3(0.0,1.0,0.0):vec3(1.0,0.0,0.0);
    vec3 tangent=normalize(cross(helper,n));
    vec3 bitangent=normalize(cross(n,tangent));
    return normalize(tangent*tangentNormal.x+bitangent*tangentNormal.y+n*tangentNormal.z);
}

bool traceImportedSurface(vec3 ro,vec3 rd,float maximumDistance,
                          out float hitDistance,out vec2 hitUv,
                          out vec3 hitNormal,out int materialHandle){
    vec3 origin=ro;float traveled=0.0,remaining=maximumDistance;
    for(int layer=0;layer<24&&remaining>TRACE_BIAS;++layer){
        float localDistance;vec2 uv;vec3 normal;int material,instanceIndex;
        if(!traceImportedNearest(origin,rd,remaining,localDistance,uv,normal,material,instanceIndex))break;
        float step=localDistance+TRACE_BIAS*0.25;
        if(traceMaterialRejectsHit(material,uv)){
            origin+=rd*step;traveled+=step;remaining-=step;continue;
        }
        int renderClass=(material>=0&&material<uTraceMaterialCount)?int(traceRecords[material].extra.w+0.5):0;
        float opacity=traceResolvedOpacity(material,uv);
        float transmission=traceResolvedTransmission(material,uv);
        float blocking=opacity*(1.0-transmission);
        if(renderClass>=2&&blocking<0.50){
            origin+=rd*step;traveled+=step;remaining-=step;continue;
        }
        hitDistance=traveled+localDistance;hitUv=uv;
        hitNormal=importedMappedNormal(material,uv,normal);materialHandle=material;
        return true;
    }
    return false;
}

bool traceScene(vec3 ro,vec3 rd,float maximumDistance,
                out int hitIndex,out float hitDistance,out vec3 hitNormal){
    int analyticIndex=-1;float analyticDistance=maximumDistance;vec3 analyticNormal=vec3(0.0);
    bool analytic=traceAnalyticScene(ro,rd,maximumDistance,analyticIndex,analyticDistance,analyticNormal);
    float importedDistance=maximumDistance;vec2 importedUv=vec2(0.0);vec3 importedNormal=vec3(0.0);int importedMaterial=-1;
    bool importedHit=traceImportedSurface(ro,rd,maximumDistance,importedDistance,importedUv,importedNormal,importedMaterial);
    if(importedHit&&(!analytic||importedDistance<analyticDistance)){
        hitIndex=-1;hitDistance=importedDistance;hitNormal=importedNormal;
        gImportedMaterial=importedMaterial;gImportedUv=importedUv;gImportedNormal=importedNormal;gImportedHit=true;return true;
    }
    gImportedMaterial=-1;gImportedUv=vec2(0.0);gImportedNormal=vec3(0.0);gImportedHit=false;
    if(analytic){hitIndex=analyticIndex;hitDistance=analyticDistance;hitNormal=analyticNormal;return true;}
    hitIndex=-1;hitDistance=maximumDistance;hitNormal=vec3(0.0);return false;
}
)GLSL";

    const std::size_t hash_at = source.find("uint hashValue");
    if (hash_at == std::string::npos)
        throw std::runtime_error("Lumen combined-trace insertion point missing");
    source.insert(hash_at, combined);

    const char *material_eval = R"GLSL(
vec3 importedFallbackRadiance(){
    if(!gImportedHit||gImportedMaterial<0||gImportedMaterial>=uTraceMaterialCount)return vec3(0.018,0.022,0.032);
    TraceRecord material=traceRecords[gImportedMaterial];
    vec3 base=traceColor(material,TRACE_SLOT_BASE_COLOR,gImportedUv,material.baseMetallic.rgb);
    vec3 emissive=traceColor(material,TRACE_SLOT_EMISSIVE,gImportedUv,material.emissiveRoughness.rgb);
    vec3 specular=traceColor(material,TRACE_SLOT_SPECULAR,gImportedUv,material.specularIor.rgb);
    float metallic=clamp(material.baseMetallic.a*traceScalar(material,TRACE_SLOT_METALLIC,gImportedUv,1.0),0.0,1.0);
    float roughness=clamp(material.emissiveRoughness.a*traceScalar(material,TRACE_SLOT_ROUGHNESS,gImportedUv,1.0),0.04,1.0);
    float reflectionTexture=traceScalar(material,TRACE_SLOT_REFLECTION,gImportedUv,1.0);
    float reflectivity=clamp(material.advanced.y*reflectionTexture,0.0,1.0);
    float clearcoat=clamp(material.advanced.z,0.0,1.0);
    vec3 diffuse=base*(1.0-metallic)*(0.030+0.025*(1.0-roughness));
    vec3 conductor=base*metallic*(0.020+0.080*reflectivity);
    vec3 spec=specular*(0.018+0.070*reflectivity+0.025*clearcoat);
    return max(emissive+diffuse+conductor+spec,vec3(0.0));
}
)GLSL";
    const std::size_t fallback_at = source.find("vec3 primitiveFallbackRadiance");
    if (fallback_at == std::string::npos)
        throw std::runtime_error("Lumen imported material insertion point missing");
    source.insert(fallback_at, material_eval);
    replaceLumenRequired(&source,
        "vec3 primitiveFallbackRadiance(int i){",
        "vec3 primitiveFallbackRadiance(int i){if(gImportedHit)return importedFallbackRadiance();");

    replaceLumenRequired(&source,
        "layout(binding=6) uniform sampler2D sPreviousPosition;",
        "layout(binding=6) uniform sampler2D sPreviousPosition;\nlayout(binding=9) uniform sampler2D sSpecularIor;\nlayout(binding=10) uniform sampler2D sAdvancedMaterial;");
    replaceLumenRequired(&source,
        "vec4 nr=texelFetch(sNormalRoughness,pixel,0),am=texelFetch(sAlbedoMetallic,pixel,0);",
        "vec4 nr=texelFetch(sNormalRoughness,pixel,0),am=texelFetch(sAlbedoMetallic,pixel,0),si=texelFetch(sSpecularIor,pixel,0),advanced=texelFetch(sAdvancedMaterial,pixel,0);");
    replaceLumenRequired(&source,
        "if(metallic>0.08||roughness<0.45)",
        "if(metallic>0.08||roughness<0.45||advanced.w>0.01||advanced.x>0.01)");
    replaceLumenRequired(&source,
        "vec3 hp=origin+reflectionDirection*rd,source=screenRadiance(hp,ri),f0=mix(vec3(0.04),albedo,metallic);",
        "vec3 hp=origin+reflectionDirection*rd,source=screenRadiance(hp,ri);float ni=max(si.a,1.0001),iorRatio=(ni-1.0)/(ni+1.0);vec3 dielectricF0=dot(si.rgb,si.rgb)>EPSILON?clamp(si.rgb,vec3(0.0),vec3(1.0)):vec3(iorRatio*iorRatio);vec3 f0=mix(dielectricF0,albedo,metallic);");
    replaceLumenRequired(&source,
        "reflection=source*weight*(1.0-roughness*0.82);",
        "float materialReflection=clamp(max(advanced.w,metallic),0.0,1.0);float clearcoat=clamp(advanced.x,0.0,1.0),clearcoatRoughness=clamp(advanced.y,0.04,1.0);reflection=source*weight*(0.18+0.82*materialReflection)*(1.0-roughness*0.75)+source*vec3(0.04)*clearcoat*(1.0-clearcoatRoughness*0.70);");

    return source;
}

} }
#endif
