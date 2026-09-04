#include "Renderer/Gpu/GBufferReconstruct.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace Renderer;

static void require(bool value,const char *message)
{
    if(value)return;
    std::fprintf(stderr,"FAIL: %s\n",message);
    std::exit(1);
}

int main()
{
    Math::Mat4 projection={};
    projection.value[0]=1.2f;
    projection.value[5]=1.7f;
    projection.value[10]=-1.01f;
    projection.value[11]=-1.0f;
    projection.value[14]=-0.201f;

    Math::Mat4 inverse={};
    require(Gpu::invertGBufferViewProjection(projection,&inverse),
            "view-projection must be invertible");

    const Math::Vec3 world={0.25f,-0.15f,-3.0f};
    Math::Vec2 uv={};
    float depth=0.0f;
    require(Gpu::projectGBufferWorldPosition(projection,world,&uv,&depth),
            "world position must project");
    const Math::Vec3 reconstructed=
        Gpu::reconstructGBufferWorldPosition(inverse,uv,depth);
    require(std::fabs(reconstructed.x-world.x)<1.0e-4f
            &&std::fabs(reconstructed.y-world.y)<1.0e-4f
            &&std::fabs(reconstructed.z-world.z)<1.0e-4f,
            "depth plus inverse view-projection must reconstruct world position");

    std::puts("gbuffer_reconstruct_contract=PASS");
    return 0;
}
