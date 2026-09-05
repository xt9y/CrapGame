#include "Renderer/Gpu/DirectLightingGpu.hpp"
#include "Renderer/Gpu/DirectLightingImportedShader.hpp"
#include "Renderer/Gpu/SmrtShadowGpu.hpp"
#include "Renderer/Gpu/VirtualShadowMapGpu.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static void require(bool value,const char *message)
{
    if(!value){std::cerr<<message<<'\n';std::exit(1);}
}

static std::string readFile(const char *path)
{
    std::ifstream file(path);
    std::ostringstream stream;
    stream<<file.rdbuf();
    return stream.str();
}

int main()
{
    const std::string header=readFile("Sources/Renderer/Gpu/DirectLightingGpu.hpp");
    const std::string shader=Renderer::Gpu::directLightingImportedShader();

    require(header.find("VirtualShadowMapGpu")!=std::string::npos,
            "direct lighting must own VSM");
    require(header.find("SmrtShadowGpu")!=std::string::npos,
            "direct lighting must own SMRT projection");
    require(header.find("StaticShadowCacheGpu")==std::string::npos,
            "active direct lighting must not own the old PCF cache");
    require(shader.find("sSmrtVisibility")!=std::string::npos,
            "direct lighting must sample SMRT visibility");
    require(shader.find("staticShadowVisibility")==std::string::npos,
            "old static PCF visibility must be absent");
    require(shader.find("importedShadowVisibility(position") == std::string::npos,
            "direct lighting must not ray-trace imported visibility after SMRT");

    std::cout<<"virtual_shadow_integration_contract=PASS\n";
}
