#include "Renderer/Gpu/FrameWorkPolicy.hpp"

#include <cstdlib>
#include <iostream>

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

    require(movingSecondaryIntervalNanoseconds(0u)==66666667ull,"default moving refresh is not 15 Hz");
    require(!movingSecondaryRefreshDue(66666666ull,1ull,0u,false),"secondary refresh fired before 15 Hz interval");
    require(movingSecondaryRefreshDue(66666668ull,1ull,0u,false),"secondary refresh missed 15 Hz interval");
    require(movingSecondaryRefreshDue(2ull,1ull,0u,true),"revision invalidation must force secondary refresh");
    require(movingSecondaryIntervalNanoseconds(30u)==33333333ull,"configured Lumen Hz must override moving interval");

    std::cout<<"static_sponza_frame_policy_contract=PASS\n";
    return 0;
}
