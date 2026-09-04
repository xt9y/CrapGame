#include "Renderer/Gpu/RuntimeHotPathV3.hpp"

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
    std::uint64_t phase=0u;
    Gpu::setSceneSimulationEnabled(false);
    require(Gpu::simulationTicksDue(1000000000ull,&phase)==0u,
            "static scenes must skip fixed-step simulation");
    require(phase==0u,"static scenes must not accumulate simulation phase");

    Gpu::setSceneSimulationEnabled(true);
    require(Gpu::simulationTicksDue(16666667ull,&phase)==1u,
            "dynamic scenes must retain 60 Hz fixed-step simulation");

    std::puts("static_scene_simulation_contract=PASS");
    return 0;
}
