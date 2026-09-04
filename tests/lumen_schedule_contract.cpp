#include "Renderer/Gpu/LumenSchedule.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition,const char *message)
{
    if(condition)return;
    std::cerr<<"lumen_schedule_contract: "<<message<<'\n';
    std::exit(1);
}
}

int main()
{
    Renderer::Gpu::LumenSchedule schedule(0u);
    require(schedule.due(0u,false),"first update must run");
    require(schedule.currentHz(0u)==60u,"initial convergence must be capped at 60 Hz");
    require(schedule.currentHz(500000000ull)==30u,"early stable convergence must drop to 30 Hz");
    require(schedule.currentHz(2000000000ull)==20u,"settled convergence must drop to 20 Hz");
    require(schedule.currentHz(4000000000ull)==15u,"steady state must cap trace work at 15 Hz");

    Renderer::Gpu::LumenSchedule fixed(72u);
    require(fixed.currentHz(0u)==72u,"fixed CRAPGAME_LUMEN_HZ policy must remain exact");

    std::cout<<"lumen_schedule_contract=PASS\n";
    return 0;
}
