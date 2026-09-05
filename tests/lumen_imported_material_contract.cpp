#include "Renderer/Gpu/LumenImportedShader.hpp"
#include "Renderer/Gpu/LumenStageAComposite.hpp"

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
    const std::string shader=Renderer::Gpu::lumenImportedTraceShader();
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

    const std::string composite=Renderer::Gpu::LUMEN_STAGE_A_COMPOSITE_COMPUTE;
    require(composite.find("acesToneMap")!=std::string::npos,
            "ACES output mapping missing");
    require(composite.find("OUTPUT_EXPOSURE")!=std::string::npos,
            "explicit output exposure missing");

    if(argc>1)
    {
        std::ofstream out(argv[1]);
        out<<shader;
    }

    std::cout<<"lumen_imported_material_contract=PASS\n";
    return 0;
}
