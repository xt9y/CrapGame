#include "Models/Texture.hpp"

#include <cstdlib>
#include <iostream>

namespace
{
void require(bool condition,const char *message)
{
    if(condition)return;
    std::cerr<<"normal_convention_contract: "<<message<<'\n';
    std::exit(1);
}
}

int main()
{
    require(Models::normalMapUsesNegativeY("textures/sponza_arch_ddn.tga"),
            "Crytek _ddn normal maps must be Y-negative");
    require(Models::normalMapUsesNegativeY("textures/tree_DDNA.TGA"),
            "Crytek _ddna detection must be case-insensitive");
    require(!Models::normalMapUsesNegativeY("textures/normal.tga"),
            "generic normal maps must stay OpenGL Y-positive");
    std::cout<<"normal_convention_contract=PASS\n";
    return 0;
}
