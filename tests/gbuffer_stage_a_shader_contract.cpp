#include "Renderer/Gpu/DirectLightingImportedShader.hpp"
#include "Renderer/Gpu/LumenImportedStageAShader.hpp"
#include "Renderer/Gpu/LumenStageAComposite.hpp"
#include "Renderer/Gpu/ReprojectionShader.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace Renderer;

static void require(bool value,const char *message)
{
    if(value)return;
    std::fprintf(stderr,"FAIL: %s\n",message);
    std::exit(1);
}

int main()
{
    const std::string direct=Gpu::directLightingImportedShader();
    const std::string lumen=Gpu::lumenImportedStageATraceShader();
    const std::string composite=Gpu::LUMEN_STAGE_A_COMPOSITE_COMPUTE;
    const std::string reprojection=Gpu::REPROJECTION_COMPUTE;

    require(direct.find("sGBufferDepth")!=std::string::npos
            &&direct.find("gbufferReconstructWorld")!=std::string::npos,
            "direct lighting must reconstruct position from depth");
    require(lumen.find("gbufferReconstructWorld")!=std::string::npos
            &&lumen.find("vec4 sp=texelFetch(sPositionDepth") == std::string::npos
            &&lumen.find("vec4 surface=texelFetch(sPositionDepth") == std::string::npos,
            "imported Lumen must not fetch a world-position texture");
    require(composite.find("uGBufferInverseViewProjection")!=std::string::npos
            &&composite.find("gbufferReconstructWorld")!=std::string::npos,
            "composite must reconstruct position from depth");
    require(reprojection.find("uCurrentInverseViewProjection")!=std::string::npos
            &&reprojection.find("sCurrentDepth")!=std::string::npos,
            "reprojection must reconstruct current position from depth");

    std::puts("gbuffer_stage_a_shader_contract=PASS");
    return 0;
}
