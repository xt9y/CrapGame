#include "Renderer/Gpu/DirectLightingImportedShader.hpp"

#include <cstdlib>
#include <exception>
#include <fstream>
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

int main(int argc,char **argv)
{
    try
    {
        const std::string source=Renderer::Gpu::directLightingImportedShader();
        require(source.find("bool traceAabbEntry(")!=std::string::npos,
                "front-to-back AABB entry helper missing");
        require(source.find("leftEntry<=rightEntry")!=std::string::npos,
                "front-to-back BVH child ordering missing");

        const std::string shadow_token="bool traceImportedShadowInstanceAny(";
        const std::string shadow_call=
            "traceImportedShadowInstanceAny(ii,ro,rd,maximumDistance)";
        const std::size_t declaration=source.find(shadow_token);
        const std::size_t call=source.find(shadow_call);
        const std::size_t definition=declaration==std::string::npos
            ?std::string::npos:source.find(shadow_token,declaration+shadow_token.size());
        require(declaration!=std::string::npos,
                "compact imported shadow traversal declaration missing");
        require(call!=std::string::npos,
                "compact imported shadow traversal call missing");
        require(definition!=std::string::npos,
                "compact imported shadow traversal definition missing");
        require(declaration<call&&call<definition,
                "compact imported shadow traversal must be declared before its caller");
        const std::size_t declaration_end=source.find(';',declaration);
        require(declaration_end!=std::string::npos&&declaration_end<call,
                "compact imported shadow traversal forward declaration is malformed");

        if(argc>1)
        {
            std::ofstream out(argv[1],std::ios::binary);
            require(out.is_open(),"failed to open generated shader output");
            out<<source;
            require(out.good(),"failed to write generated shader output");
        }
    }
    catch(const std::exception& exception)
    {
        std::cerr<<exception.what()<<'\n';
        return 1;
    }

    std::cout<<"direct_lighting_imported_shader_contract=PASS\n";
    return 0;
}
