#include "Renderer/Gpu/FrameWorkPolicy.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

using namespace Renderer::Gpu;

static void require(bool condition,const char *message)
{
    if(!condition){std::cerr<<message<<'\n';std::exit(1);}
}

int main()
{
    RevisionState r={};
    FrameWork stationary=decideFrameWork(r,r,true,false,false);
    require(!stationary.geometry&&!stationary.shadow&&!stationary.static_diffuse&&!stationary.view_specular,"converged static frame scheduled direct work");
    require(!stationary.lumen_trace&&!stationary.composite&&!stationary.transparent&&stationary.present,"converged static frame must only present");

    FrameWork transparent=decideFrameWork(r,r,true,false,true);
    require(transparent.transparent&&transparent.composite,"dynamic transparency must force composition");

    RevisionState camera=r;camera.camera=1u;
    FrameWork moving=decideFrameWork(r,camera,false,true,false);
    require(moving.geometry&&moving.reprojection&&moving.dirty_tiles&&moving.view_specular,"camera motion must schedule view/reprojection work");
    require(!moving.shadow,"camera motion invalidated static shadow cache");

    RevisionState resized=r;resized.resolution=1u;
    FrameWork resizeWork=decideFrameWork(r,resized,false,false,false);
    require(resizeWork.geometry&&!resizeWork.shadow,"resolution change must redraw without invalidating world shadow cache");

    RevisionState light=camera;light.lighting=1u;
    FrameWork relight=decideFrameWork(camera,light,false,false,false);
    require(relight.shadow&&relight.static_diffuse&&relight.lumen_trace,"lighting invalidation did not schedule affected work");
    require(!relight.reprojection,"lighting invalidation must not be treated as camera-only reprojection");

    require(movingSecondaryIntervalNanoseconds(0u)==66666667ull,"reference 15 Hz interval changed");
    require(movingSecondaryRefreshDue(2ull,1ull,0u,false),
            "default moving GI must launch one budgeted secondary slice per rendered frame");
    require(!movingSecondaryRefreshDue(33333333ull,1ull,30u,false),
            "explicit 30 Hz secondary throttle fired early");
    require(movingSecondaryRefreshDue(33333334ull,1ull,30u,false),
            "explicit 30 Hz secondary throttle missed its interval");
    require(movingSecondaryRefreshDue(2ull,1ull,30u,true),
            "revision invalidation must force secondary refresh");
    require(movingSecondaryIntervalNanoseconds(30u)==33333333ull,"configured Lumen Hz interval changed");

    std::ifstream main_source("Sources/main.cpp");
    std::string source((std::istreambuf_iterator<char>(main_source)),{});
    require(source.find("glfwGetFramebufferSize")!=std::string::npos,
            "interactive renderer is not sized from the physical framebuffer");
    require(source.find("queryFramebufferExtent")!=std::string::npos,
            "framebuffer extent helper missing from interactive path");

    std::cout<<"static_sponza_frame_policy_contract=PASS\n";
    return 0;
}
