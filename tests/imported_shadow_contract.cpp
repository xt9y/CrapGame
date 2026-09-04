#include "Renderer/Gpu/DirectLightingImportedShader.hpp"

#include <cassert>
#include <iostream>
#include <string>

int main()
{
    const std::string shader=Renderer::Gpu::directLightingImportedShader();
    assert(shader.find("traceImportedNearest")!=std::string::npos);
    assert(shader.find("importedShadowVisibility")!=std::string::npos);
    assert(shader.find("traceMaterialRejectsHit")!=std::string::npos);
    assert(shader.find("shadowed(position,normal,ld,maxD)")!=std::string::npos);
    assert(shader.find("radiance*=importedVisibility")!=std::string::npos);
    assert(shader.find("renderClass==1&&traceResolvedOpacity")!=std::string::npos);
    assert(shader.find("if(dot(normal,ld)<=0.0)continue;")!=std::string::npos);
    assert(shader.find("traceImportedOpaqueAny")!=std::string::npos);
    assert(shader.find("if(traceImportedOpaqueAny(ro,rd,maximumDistance))return 0.0;")
        !=std::string::npos);
    std::cout<<"imported_shadow_contract=PASS\n";
    return 0;
}
