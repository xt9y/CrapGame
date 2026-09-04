#include "Renderer/Gpu/DirectLightingImportedShader.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

static void require(bool condition,const char *message)
{
    if(!condition)
    {
        std::cerr<<message<<'\n';
        std::exit(1);
    }
}

int main()
{
    try
    {
        const std::string source=Renderer::Gpu::directLightingImportedShader();
        require(source.find("bool traceAabbEntry(")!=std::string::npos,
                "front-to-back AABB entry helper missing");
        require(source.find("leftEntry<=rightEntry")!=std::string::npos,
                "front-to-back BVH child ordering missing");
        require(source.find("bool traceImportedShadowInstanceAny(")!=std::string::npos,
                "compact imported shadow traversal missing");
    }
    catch(const std::exception& exception)
    {
        std::cerr<<exception.what()<<'\n';
        return 1;
    }

    std::cout<<"direct_lighting_imported_shader_contract=PASS\n";
    return 0;
}
