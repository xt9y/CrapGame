#include "Renderer/Gpu/SmrtShadowPolicy.hpp"
#include "Renderer/Gpu/SmrtShadowShader.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

static void require(bool value,const char *message)
{
    if(!value){std::cerr<<message<<'\n';std::exit(1);}
}

int main()
{
    using namespace Renderer::Gpu;

    require(SmrtShadowPolicy::DIRECTIONAL_RAYS==7,
            "SMRT directional ray count");
    require(SmrtShadowPolicy::DIRECTIONAL_SAMPLES_PER_RAY==8,
            "SMRT directional samples per ray");
    require(SmrtShadowPolicy::LOCAL_RAYS==7,
            "SMRT local ray count");
    require(SmrtShadowPolicy::LOCAL_SAMPLES_PER_RAY==8,
            "SMRT local samples per ray");
    require(SmrtShadowPolicy::ADAPTIVE_RAY_COUNT,
            "adaptive SMRT ray count");

    const std::string shader=SMRT_SHADOW_GLSL;
    require(shader.find("DIRECTIONAL_RAYS")!=std::string::npos,
            "directional SMRT loop missing");
    require(shader.find("SAMPLES_PER_RAY")!=std::string::npos,
            "SMRT depth sampling missing");
    require(shader.find("frameIndex")!=std::string::npos,
            "temporal SMRT rotation missing");
    require(shader.find("sampleDisk")!=std::string::npos,
            "finite source disk sampling missing");
    require(shader.find("blockerDistance")!=std::string::npos,
            "blocker distance accumulation missing");
    require(shader.find("SCREEN_RAY_LENGTH")!=std::string::npos,
            "screen smart-bias stage missing");
    require(shader.find("NORMAL_BIAS")!=std::string::npos,
            "normal receiver bias missing");
    require(shader.find("DIRECTIONAL_EXTRAPOLATE_MAX_SLOPE")!=std::string::npos,
            "directional blocker extrapolation missing");
    require(shader.find("LOCAL_EXTRAPOLATE_MAX_SLOPE")!=std::string::npos,
            "local blocker extrapolation missing");
    require(shader.find("sampleVirtualShadowPage")!=std::string::npos,
            "hierarchical VSM sampling missing");
    require(shader.find("virtualShadowVisibility")!=std::string::npos,
            "SMRT visibility entry point missing");
    require(shader.find("for (int y = -1") == std::string::npos,
            "fixed PCF loop must not exist in SMRT");

    std::cout<<"smrt_shadow_contract=PASS\n";
}
