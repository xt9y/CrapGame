#include "Renderer/Gpu/DirectLightingImportedShader.hpp"
#include "Renderer/Gpu/StaticDirectSplitPolicy.hpp"

#include <cctype>
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
    std::ifstream input(path);
    std::ostringstream stream;
    stream<<input.rdbuf();
    return stream.str();
}

static std::string compact(std::string value)
{
    std::string result;
    result.reserve(value.size());
    for(char c:value)
        if(!std::isspace(static_cast<unsigned char>(c)))result.push_back(c);
    return result;
}

int main()
{
    using namespace Renderer::Gpu;

    RevisionState cached{};
    cached.geometry=1u;cached.material=2u;cached.lighting=3u;cached.camera=4u;
    cached.resolution=5u;cached.mesh_registry=6u;cached.material_registry=7u;
    RevisionState current=cached;

    current.camera++;
    require(staticDiffuseValid(cached,current),
            "camera change must preserve the static diffuse lighting solution");
    require(!viewSpecularValid(cached,current),
            "camera change must invalidate view-dependent specular");

    current=cached;current.geometry++;
    require(!staticDiffuseValid(cached,current),"geometry must invalidate static diffuse");
    current=cached;current.material++;
    require(!staticDiffuseValid(cached,current),"material must invalidate static diffuse");
    current=cached;current.lighting++;
    require(!staticDiffuseValid(cached,current),"lighting must invalidate static diffuse");

    const std::string scene=compact(readFile("Sources/Renderer/Gpu/DirectLightingScene.cpp"));
    require(scene.find("static_diffuse_.invalidate();")!=std::string::npos,
            "scheduled direct work must refresh the screen-space static diffuse projection");
    require(scene.find("static_diffuse_.texture()")!=std::string::npos
            &&scene.find("view_specular_.texture()")!=std::string::npos
            &&scene.find("dynamic_color_")!=std::string::npos,
            "final direct term must combine static diffuse, view specular, and dynamic light");

    const std::string imported=compact(readFile("Sources/Renderer/Gpu/DirectLightingImported.cpp"));
    require(imported.find("!primitives_.empty()")!=std::string::npos,
            "mixed procedural scenes must retain the exact legacy static-light path");

    const std::string shader=compact(directLightingImportedShader());
    require(shader.find("if(i==uStaticSplitLightIndex)continue")!=std::string::npos,
            "dynamic direct pass must skip only the split static light");
    require(shader.find("vec3direct=vec3(0.0)")!=std::string::npos,
            "dynamic direct pass must not duplicate emissive or ambient terms");
    require(shader.find("eo=imageLoad(gEmissive,pixel)")==std::string::npos,
            "dynamic direct pass must not retain the dead emissive GBuffer read");

    const std::string static_diffuse=compact(readFile("Sources/Renderer/Gpu/StaticDiffuseLightingGpu.cpp"));
    require(static_diffuse.find("layout(std430,binding=8)readonlybufferLightBuffer")!=std::string::npos
            &&static_diffuse.find("glBindBufferBase(GL_SHADER_STORAGE_BUFFER,8,light_buffer_)")!=std::string::npos,
            "static diffuse light data must not clobber imported traversal SSBO bindings");
    require(static_diffuse.find("vec3lambert=albedo*(1.0-metallic)/PI")!=std::string::npos,
            "static pass must own the camera-independent Lambert term");
    require(static_diffuse.find("eo.xyz+ambientTransmission.rgb*albedo*0.025")!=std::string::npos,
            "static pass must own emissive and ambient terms");

    const std::string view=compact(readFile("Sources/Renderer/Gpu/ViewSpecularGpu.cpp"));
    require(view.find("layout(std430,binding=8)readonlybufferLightBuffer")!=std::string::npos
            &&view.find("glBindBufferBase(GL_SHADER_STORAGE_BUFFER,8,light_buffer_)")!=std::string::npos,
            "view specular light data must not clobber imported traversal SSBO bindings");
    require(view.find("vec3full=evaluateMaterialPbr")!=std::string::npos
            &&view.find("full-lambert")!=std::string::npos,
            "view pass must preserve exact PBR by emitting the full-minus-Lambert remainder");

    std::cout<<"static_direct_split_contract=PASS\n";
    return 0;
}
