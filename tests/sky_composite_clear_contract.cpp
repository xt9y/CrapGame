#include "Renderer/Gpu/FrameHotPath.hpp"

#include <cstdlib>
#include <iostream>

static void require (bool value, const char *message)
{
    if (!value)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

int main ()
{
    using Renderer::Gpu::lumenCompositeRequired;

    require(
        lumenCompositeRequired(true, false),
        "direct/background changes require a fresh composite even without a Lumen trace"
    );
    require(
        lumenCompositeRequired(false, true),
        "new Lumen history requires a fresh composite"
    );
    require(
        !lumenCompositeRequired(false, false),
        "fully unchanged frame reuses the previous final texture"
    );

    std::cout << "sky_composite_clear_contract=PASS\n";
    return 0;
}
