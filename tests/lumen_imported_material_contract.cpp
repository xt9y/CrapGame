#include "Renderer/Gpu/ConvergencePolicy.hpp"
#include "Renderer/Gpu/LumenImportedStageAShader.hpp"
#include "Renderer/Gpu/LumenStageAComposite.hpp"
#include "Renderer/Gpu/RadianceCachePolicy.hpp"
#include "Renderer/Gpu/ReflectionCacheShader.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
void require(bool condition,const char *message)
{
    if(condition)return;
    std::cerr<<"lumen_imported_material_contract: "<<message<<'\n';
    std::exit(1);
}
}

int main(int argc,char **argv)
{
    const std::string shader=Renderer::Gpu::lumenImportedStageATraceShader();
    require(shader.find("traceImportedNearest")!=std::string::npos,
            "imported TLAS/BLAS traversal missing");
    require(shader.find("traceMaterialRejectsHit")!=std::string::npos,
            "alpha-aware imported continuation missing");
    require(shader.find("TRACE_SLOT_BASE_COLOR")!=std::string::npos,
            "textured imported base color missing");
    require(shader.find("TRACE_SLOT_EMISSIVE")!=std::string::npos,
            "textured imported emissive missing");
    require(shader.find("TRACE_SLOT_METALLIC")!=std::string::npos,
            "textured imported metallic missing");
    require(shader.find("TRACE_SLOT_ROUGHNESS")!=std::string::npos,
            "textured imported roughness missing");
    require(shader.find("TRACE_SLOT_REFLECTION")!=std::string::npos,
            "imported reflectivity texture missing");
    require(shader.find("importedMappedNormal")!=std::string::npos,
            "imported normal-map response missing");
    require(shader.find("layout(std430,binding=5) readonly buffer PrimitiveBuffer")!=std::string::npos,
            "analytic primitive binding conflicts with imported scene");
    require(shader.find("layout(binding=7) uniform sampler2DArray sTraceColorAtlas")!=std::string::npos,
            "trace color atlas not moved out of Lumen history units");
    require(shader.find("layout(binding=8) uniform sampler2DArray sTraceDataAtlas")!=std::string::npos,
            "trace data atlas not moved out of Lumen history units");
    require(shader.find("sSpecularIor")!=std::string::npos,
            "primary reflection specular/IOR input missing");
    require(shader.find("sAdvancedMaterial")!=std::string::npos,
            "primary reflection advanced-material input missing");
    require(shader.find("materialReflection")!=std::string::npos,
            "material-weighted reflection missing");
    require(shader.find("clearcoatRoughness")!=std::string::npos,
            "clearcoat reflection response missing");

    require(shader.find("layout(std430,binding=10) readonly buffer LumenLightBuffer")!=std::string::npos,
            "world-space GI light buffer missing");
    require(shader.find("uniform int uLightCount")!=std::string::npos,
            "world-space GI light count missing");
    require(shader.find("lumenWorldHitRadiance")!=std::string::npos,
            "secondary hits do not evaluate world-space radiance");
    require(shader.find("coneShadow.w")!=std::string::npos,
            "light indirect intensity is not applied to GI");
    require(shader.find("lumenEnvironmentRadiance")!=std::string::npos,
            "environment radiance source missing");
    require(shader.find("indirect=giSource*albedo*(1.0-metallic);")!=std::string::npos,
            "cosine-weighted diffuse estimator still loses a pi factor");

    require(Renderer::Gpu::ConvergencePolicy::DEFAULT_SAMPLES>=48u,
            "static GI freezes before enough temporal samples accumulate");
    require(Renderer::Gpu::ConvergencePolicy::MAX_SAMPLES
                >=Renderer::Gpu::ConvergencePolicy::DEFAULT_SAMPLES,
            "configured convergence cap is below the default");
    require(Renderer::Gpu::RadianceCachePolicy::ACCEPT_CONFIDENCE>=16u,
            "radiance cache becomes readable while still too noisy");
    require(Renderer::Gpu::RadianceCachePolicy::HIGH_CONFIDENCE_SAMPLES>=48u,
            "radiance cache cannot refine through the static convergence window");
    require(Renderer::Gpu::RadianceCachePolicy::ACCEPT_CONFIDENCE
                < Renderer::Gpu::RadianceCachePolicy::HIGH_CONFIDENCE_SAMPLES,
            "radiance cache must keep refining after it becomes readable");
    require(shader.find("cachedGiSource")!=std::string::npos,
            "readable radiance cache stops stochastic refinement");
    require(shader.find("if(giCached)giSource=cachedGiSource")!=std::string::npos,
            "refined cache is not used as the stable GI result");
    require(shader.find("bestHistoryPixel,0).xyz,0.94")!=std::string::npos,
            "temporal GI history is too short to suppress visible variance");
    require(shader.find("lumenEnvironmentIrradiance")!=std::string::npos,
            "secondary-hit skylight irradiance missing");
    require(shader.find("lumenGroundRadiance")!=std::string::npos,
            "neutral ground skylight source missing");
    require(shader.find("return mix(lower,upper,up)*0.42")!=std::string::npos,
            "secondary-hit skylight remains too weak for Sponza interiors");

    const std::string reflectionCache=Renderer::Gpu::REFLECTION_CACHE_GLSL;
    const std::size_t roughCache=reflectionCache.find(
        "if(roughness>=0.35&&radianceCacheLookup(position,normal,source))return true;");
    const std::size_t sharpScreen=reflectionCache.find(
        "if(roughness<0.35&&reflectionScreenHit(origin,direction,source))return true;");
    require(roughCache!=std::string::npos,
            "rough reflections do not prefer the stable radiance cache");
    require(sharpScreen!=std::string::npos,
            "sharp screen-space reflections are still enabled on rough surfaces");
    require(roughCache<sharpScreen,
            "rough reflection cache must be evaluated before sharp SSR");

    const std::string composite=Renderer::Gpu::LUMEN_STAGE_A_COMPOSITE_COMPUTE;
    require(composite.find("acesToneMap")!=std::string::npos,
            "ACES output mapping missing");
    require(composite.find("OUTPUT_EXPOSURE")!=std::string::npos,
            "explicit output exposure missing");
    require(composite.find("bilateralIndirect")!=std::string::npos,
            "depth/normal-aware indirect denoising missing");
    require(composite.find("offsets[9]")!=std::string::npos,
            "indirect denoiser does not cover a full 3x3 neighborhood");
    require(composite.find("normalWeight")!=std::string::npos,
            "indirect filter is not normal aware");
    require(composite.find("positionWeight")!=std::string::npos,
            "indirect filter is not depth/position aware");
    require(composite.find("bilateralReflection")!=std::string::npos,
            "half-resolution reflections are still nearest-upsampled");
    require(composite.find("roughnessWeight")!=std::string::npos,
            "reflection filter is not roughness aware");
    require(composite.find("reflectionFireflyClamp")!=std::string::npos,
            "isolated reflection fireflies are not bounded before composition");
    require(composite.find("occlusion*0.12")!=std::string::npos,
            "short-range AO is strong enough to create false shadow blotches");

    if(argc>1)
    {
        std::ofstream out(argv[1]);
        out<<shader;
    }
    if(argc>2)
    {
        std::ofstream out(argv[2]);
        out<<composite;
    }

    std::cout<<"lumen_imported_material_contract=PASS\n";
    return 0;
}
