#include "Renderer/Gpu/DirectLightingImportedShader.hpp"
#include "Renderer/Gpu/StaticShadowPolicy.hpp"
#include "Renderer/Gpu/StaticShadowShader.hpp"

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
    assert(shader.find("?staticShadowVisibility(position)")!=std::string::npos);

    assert(Renderer::Gpu::StaticShadowPolicy::PCF_RADIUS==2);
    const std::string staticShadow=Renderer::Gpu::STATIC_SHADOW_DIRECT_GLSL;
    assert(staticShadow.find("staticShadowVisibility(vec3 position)")!=std::string::npos);
    assert(staticShadow.find("for (int y = -1; y <= 2; ++y)")!=std::string::npos);
    assert(staticShadow.find("for (int x = -1; x <= 2; ++x)")!=std::string::npos);
    assert(staticShadow.find("fract(texelPosition)")!=std::string::npos);
    assert(staticShadow.find("texelFetch(sStaticShadow")!=std::string::npos);
    assert(staticShadow.find("texture(sStaticShadow") == std::string::npos);

    std::cout<<"imported_shadow_contract=PASS\n";
    return 0;
}
