#include "Renderer/Gpu/RevisionState.hpp"
#include "Renderer/Gpu/StaticShadowPolicy.hpp"
#include "Renderer/Gpu/StaticShadowShader.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

static void require(bool value,const char* message)
{
    if(!value){std::cerr<<message<<'\n';std::exit(1);}
}

int main()
{
    using namespace Renderer::Gpu;
    require(StaticShadowPolicy::SIZE==2048,"static shadow atlas must be 2048x2048");
    require(StaticShadowPolicy::PCF_RADIUS==2,"static shadow filter must use radius-two soft PCF");
    require(StaticShadowPolicy::PADDING==0.05f,"light fit must retain five-percent padding");

    RevisionState cached{};
    cached.geometry=3;cached.material=7;cached.lighting=2;
    cached.camera=11;cached.resolution=1;
    cached.mesh_registry=9;cached.material_registry=5;

    RevisionState camera=cached;++camera.camera;++camera.resolution;
    require(staticShadowValid(cached,camera),"camera/viewport motion must retain static shadow cache");
    RevisionState geometry=cached;++geometry.geometry;
    require(!staticShadowValid(cached,geometry),"geometry invalidates static shadow cache");
    RevisionState material=cached;++material.material;
    require(!staticShadowValid(cached,material),"material invalidates masked shadow cache");
    RevisionState lighting=cached;++lighting.lighting;
    require(!staticShadowValid(cached,lighting),"light changes invalidate static shadow cache");

    const std::string direct=STATIC_SHADOW_DIRECT_GLSL;
    require(direct.find("for (int y = -2; y <= 2")!=std::string::npos,
            "direct lookup must use a radius-two PCF kernel");
    require(direct.find("float weight = float(3 - abs(x)) * float(3 - abs(y));")!=std::string::npos,
            "PCF must use deterministic tent weights");
    require(direct.find("texelFetch(sStaticShadow")!=std::string::npos,
            "manual depth comparisons must bypass texture filtering");
    require(direct.find("layout(binding=6)")!=std::string::npos,
            "static shadow cache must use the reserved direct-light texture unit");

    std::cout<<"static_shadow_cache_contract=PASS\n";
    return 0;
}
