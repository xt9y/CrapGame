#include "Renderer/Gpu/LumenImportedShader.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
void require(bool condition,const char *message)
{
    if(condition)return;
    std::cerr<<"lumen_history_neighborhood_contract: "<<message<<'\n';
    std::exit(1);
}
}

int main()
{
    const std::string shader=Renderer::Gpu::lumenImportedTraceShader();
    require(shader.find("bestHistoryPixel")!=std::string::npos,
            "temporal history must search neighboring previous pixels");
    require(shader.find("bestHistoryDistance")!=std::string::npos,
            "temporal history must choose the closest world-space match");
    require(shader.find("texelFetch(sPreviousPosition,tp,0)")==std::string::npos,
            "same-pixel-only history must not survive imported motion stabilization");
    std::cout<<"lumen_history_neighborhood_contract=PASS\n";
    return 0;
}
