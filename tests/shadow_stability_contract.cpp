#include "Renderer/Gpu/StaticShadowPolicy.hpp"
#include "Renderer/Gpu/StaticShadowShader.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

static void require(bool condition,const char *message)
{
    if(!condition){std::cerr<<"shadow_stability_contract: "<<message<<'\n';std::exit(1);}
}

static std::string readFile(const char *path)
{
    std::ifstream input(path);
    return std::string((std::istreambuf_iterator<char>(input)),{});
}

int main()
{
    using namespace Renderer::Gpu;

    require(StaticShadowPolicy::PCF_RADIUS==2,"soft shadow radius changed unexpectedly");
    require(StaticShadowPolicy::RASTER_SLOPE_BIAS>0.0f,"caster slope bias missing");
    require(StaticShadowPolicy::RASTER_CONSTANT_BIAS>0.0f,"caster constant bias missing");

    const std::string canonical=STATIC_SHADOW_VISIBILITY_GLSL;
    require(canonical.find("staticShadowVisibility(vec3 position)")!=std::string::npos,
            "visibility must not derive receiver bias from the normal-mapped shading normal");
    require(canonical.find("texture(sStaticShadow")==std::string::npos,
            "shadow comparison must not interpolate depth values before comparing");
    require(canonical.find("texelFetch(sStaticShadow")!=std::string::npos,
            "shadow comparison must use explicit depth texels");
    require(canonical.find("fract(texelPosition)")!=std::string::npos,
            "PCF weights must move smoothly with sub-texel receiver position");
    require(canonical.find("for (int y = -1; y <= 2; ++y)")!=std::string::npos,
            "stable tent PCF vertical support missing");
    require(canonical.find("for (int x = -1; x <= 2; ++x)")!=std::string::npos,
            "stable tent PCF horizontal support missing");

    const std::string diffuse=readFile("Sources/Renderer/Gpu/StaticDiffuseLightingGpu.cpp");
    const std::string specular=readFile("Sources/Renderer/Gpu/ViewSpecularGpu.cpp");
    for(const std::string *source:{&diffuse,&specular})
    {
        require(source->find("STATIC_SHADOW_VISIBILITY_GLSL")!=std::string::npos,
                "split sun pass is not using the canonical shadow visibility function");
        require(source->find("float staticShadowVisibility(")==std::string::npos,
                "split sun pass still contains a divergent shadow implementation");
        require(source->find("texture(sStaticShadow")==std::string::npos,
                "split sun pass still interpolates raw shadow depth");
    }

    const std::string cache=readFile("Sources/Renderer/Gpu/StaticShadowCacheGpu.cpp");
    require(cache.find("GL_TEXTURE_MIN_FILTER, GL_NEAREST")!=std::string::npos,
            "manual PCF shadow map must use nearest depth sampling");
    require(cache.find("glEnable(GL_POLYGON_OFFSET_FILL)")!=std::string::npos,
            "shadow caster raster offset is not enabled");
    require(cache.find("glPolygonOffset(")!=std::string::npos,
            "shadow caster raster offset values are not applied");
    require(cache.find("glDisable(GL_POLYGON_OFFSET_FILL)")!=std::string::npos,
            "shadow caster raster offset state is not restored");

    std::cout<<"shadow_stability_contract=PASS\n";
    return 0;
}
