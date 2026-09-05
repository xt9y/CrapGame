#include "Renderer/Gpu/DirectLightingImportedShader.hpp"
#include "Renderer/Gpu/StaticShadowPolicy.hpp"
#include "Renderer/Gpu/StaticShadowShader.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
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

    assert(Renderer::Gpu::StaticShadowPolicy::PCF_RADIUS==2);
    const std::string staticShadow=Renderer::Gpu::STATIC_SHADOW_DIRECT_GLSL;
    assert(staticShadow.find("for (int y = -2; y <= 2; ++y)")!=std::string::npos);
    assert(staticShadow.find("for (int x = -2; x <= 2; ++x)")!=std::string::npos);
    assert(staticShadow.find("float weight = float(3 - abs(x)) * float(3 - abs(y));")
        !=std::string::npos);

    std::ifstream shadowSource("Sources/Renderer/Gpu/StaticShadowCacheGpu.cpp");
    std::ostringstream sourceBuffer;
    sourceBuffer<<shadowSource.rdbuf();
    const std::string source=sourceBuffer.str();
    assert(source.find("GL_TEXTURE_MIN_FILTER, GL_NEAREST")!=std::string::npos);
    assert(source.find("GL_TEXTURE_MAG_FILTER, GL_NEAREST")!=std::string::npos);

    std::cout<<"imported_shadow_contract=PASS\n";
    return 0;
}
