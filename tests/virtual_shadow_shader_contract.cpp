#include "Renderer/Gpu/VirtualShadowMapShader.hpp"

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
    const std::string shader=VIRTUAL_SHADOW_MARK_COMPUTE;
    require(shader.find("local_size_x=8")!=std::string::npos,
            "8x8 VSM compute tile missing");
    require(shader.find("gbufferReconstructWorld")!=std::string::npos,
            "receiver world reconstruction missing");
    require(shader.find("RECEIVER_MASK_SIZE")!=std::string::npos,
            "8x8 receiver mask missing");
    require(shader.find("PAGE_SIZE")!=std::string::npos,
            "128-texel page math missing");
    require(shader.find("FIRST_COARSE_LEVEL")!=std::string::npos,
            "coarse page marking missing");
    require(shader.find("atomic")!=std::string::npos,
            "GPU page request allocation missing");
    require(shader.find("previousRequested")!=std::string::npos,
            "pressure history missing");
    require(shader.find("for(int page=0") == std::string::npos,
            "receiver marking must not sweep all world pages");
    std::cout<<"virtual_shadow_shader_contract=PASS\n";
}
