#include "Renderer/Gpu/ReprojectionPolicy.hpp"
#include "Renderer/Gpu/ReprojectionShader.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static void require(bool value,const char* message)
{
    if(!value){std::cerr<<message<<'\n';std::exit(1);}
}

static std::string readFile(const char* path)
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
        if(c!=' '&&c!='\t'&&c!='\n'&&c!='\r')result.push_back(c);
    return result;
}

int main()
{
    using namespace Renderer::Gpu;
    require(std::fabs(ReprojectionPolicy::positionTolerance(1.0f)-0.03f)<1.0e-6f,
            "near tolerance floor");
    require(std::fabs(ReprojectionPolicy::positionTolerance(10.0f)-0.10f)<1.0e-6f,
            "distance-scaled tolerance");

    ReprojectionValidationInput sample{};
    sample.projected_u=0.5f;sample.projected_v=0.5f;
    sample.previous_depth_valid=true;
    sample.current_material_id=7u;sample.previous_material_id=7u;
    sample.position_error=0.02f;sample.camera_distance=1.0f;
    sample.normal_dot=0.98f;
    require(ReprojectionPolicy::accepts(sample),"valid reprojection accepted");

    auto bad=sample;bad.projected_u=-0.001f;
    require(!ReprojectionPolicy::accepts(bad),"outside uv rejected");
    bad=sample;bad.previous_depth_valid=false;
    require(!ReprojectionPolicy::accepts(bad),"invalid previous depth rejected");
    bad=sample;bad.previous_material_id=8u;
    require(!ReprojectionPolicy::accepts(bad),"material mismatch rejected");
    bad=sample;bad.position_error=0.031f;
    require(!ReprojectionPolicy::accepts(bad),"position mismatch rejected");
    bad=sample;bad.normal_dot=0.939f;
    require(!ReprojectionPolicy::accepts(bad),"normal mismatch rejected");

    const std::string reprojection=REPROJECTION_COMPUTE;
    require(reprojection.find("usampler2D sCurrentMaterial")!=std::string::npos,
            "current material identity must be integer sampled");
    require(reprojection.find("usampler2D sPreviousMaterial")!=std::string::npos,
            "previous material identity must be integer sampled");
    require(reprojection.find("layout(r8ui,binding=2)")!=std::string::npos,
            "reprojection must emit an integer validity mask");
    require(reprojection.find("uPreviousViewProjection")!=std::string::npos,
            "reprojection must project into the previous camera");
    require(reprojection.find("normalDot < 0.94")!=std::string::npos,
            "normal validation threshold must be 0.94");

    const std::string capture=REPROJECTION_CAPTURE_COMPUTE;
    require(capture.find("layout(r32ui,binding=2)")!=std::string::npos,
            "history capture must retain exact R32UI material identity");

    const std::string gbuffer=compact(readFile("Sources/Renderer/Gpu/GBufferGpu.cpp"));
    require(gbuffer.find("layout(r32ui,binding=0)writeonlyuniformuimage2DoMaterialIdentity")
                !=std::string::npos,
            "GBuffer must emit exact integer material identity");
    require(gbuffer.find("GL20.glDrawBuffers(8,outputs)")!=std::string::npos,
            "material identity must not require a ninth framebuffer draw buffer");
    require(gbuffer.find("GL42.glBindImageTexture(0,material_identity_")!=std::string::npos,
            "material identity must be written through a fragment image binding");

    const std::string imported=compact(readFile("Sources/Renderer/Gpu/LumenImportedScene.cpp"));
    require(imported.find("constboolscene_unchanged")!=std::string::npos,
            "camera reprojection must be gated by semantic scene stability");
    require(imported.find("mesh_revision==reprojection_mesh_revision_")!=std::string::npos,
            "mesh registry changes must invalidate reprojection");
    require(imported.find("material_revision==reprojection_material_revision_")!=std::string::npos,
            "material registry changes must invalidate reprojection");
    require(imported.find("constbooltemporal_history_valid=history_valid_&&scene_unchanged&&!camera_changed")
                !=std::string::npos,
            "same-pixel temporal history must not survive semantic invalidation");
    require(imported.find("trace_reprojection_available_location_")!=std::string::npos,
            "imported Lumen must explicitly gate reprojected history");

    std::cout<<"reprojection_cache_contract=PASS\n";
    return 0;
}
