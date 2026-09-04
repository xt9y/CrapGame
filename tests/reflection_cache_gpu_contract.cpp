#include "Renderer/Gpu/LumenImportedShader.hpp"
#include "Renderer/Gpu/ReflectionCachePolicy.hpp"

#include <cassert>
#include <string>

using Renderer::Gpu::ReflectionCachePolicy;
using Renderer::Gpu::ReflectionHistoryValidation;
using Renderer::Gpu::ReflectionSource;

int main()
{
    assert(ReflectionCachePolicy::select(true,true,true)==ReflectionSource::ScreenSpace);
    assert(ReflectionCachePolicy::select(false,true,true)==ReflectionSource::RadianceCache);
    assert(ReflectionCachePolicy::select(false,false,true)==ReflectionSource::PreviousHistory);
    assert(ReflectionCachePolicy::select(false,false,false)==ReflectionSource::ImportedTrace);

    ReflectionHistoryValidation sample={};
    sample.previous_valid=true;
    sample.current_material_id=17u;
    sample.previous_material_id=17u;
    sample.camera_distance=8.0f;
    sample.position_error=0.08f;
    sample.normal_dot=0.96f;
    sample.current_roughness=0.20f;
    sample.previous_roughness=0.25f;
    assert(ReflectionCachePolicy::historyValid(sample));

    sample.previous_valid=false;
    assert(!ReflectionCachePolicy::historyValid(sample));
    sample.previous_valid=true;
    sample.previous_material_id=18u;
    assert(!ReflectionCachePolicy::historyValid(sample));
    sample.previous_material_id=17u;
    sample.position_error=0.081f;
    assert(!ReflectionCachePolicy::historyValid(sample));
    sample.position_error=0.08f;
    sample.normal_dot=0.959f;
    assert(!ReflectionCachePolicy::historyValid(sample));
    sample.normal_dot=0.96f;
    sample.previous_roughness=0.251f;
    assert(!ReflectionCachePolicy::historyValid(sample));

    assert(!ReflectionCachePolicy::useRadianceCache(0.349f));
    assert(ReflectionCachePolicy::useRadianceCache(0.35f));
    assert(ReflectionCachePolicy::usePreviousHistory(0.349f));
    assert(!ReflectionCachePolicy::usePreviousHistory(0.35f));

    const std::string shader=Renderer::Gpu::lumenImportedTraceShader();
    const std::size_t chooser=shader.find("bool reflectionFallbackSource");
    const std::size_t screen=shader.find("reflectionScreenHit(origin,direction,source)",chooser);
    const std::size_t radiance=shader.find("radianceCacheLookup(position,normal,source)",chooser);
    const std::size_t history=shader.find("reflectionHistorySample(position,normal,roughness,materialId,source)",chooser);
    const std::size_t imported=shader.find("traceScene(origin,direction,48.0",chooser);
    assert(chooser!=std::string::npos);
    assert(screen<radiance&&radiance<history&&history<imported);
    assert(shader.find("layout(binding=14) uniform usampler2D sMaterialIdentity")!=std::string::npos);
    assert(shader.find("dot(normalize(previousNormalRoughness.xyz),normalize(normal))<0.96")!=std::string::npos);
    assert(shader.find("abs(previousNormalRoughness.w-roughness)>0.05")!=std::string::npos);
    assert(shader.find("texelFetch(sReflectionPreviousMaterial,historyPixel,0).r!=materialId")!=std::string::npos);
    assert(shader.find("vec3 hp=origin+reflectionDirection*rd") == std::string::npos);
    return 0;
}
