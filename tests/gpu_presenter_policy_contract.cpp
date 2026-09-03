#include "Renderer/Gpu/PresenterPolicy.hpp"

#include <cassert>

int main ()
{
    using namespace Renderer::Gpu;

    assert(!glValidationEnabled(nullptr));
    assert(!glValidationEnabled(""));
    assert(!glValidationEnabled("0"));
    assert(glValidationEnabled("1"));

    assert(presenterStateNeedsBind(false, 7u, 7u, false, false));
    assert(!presenterStateNeedsBind(true, 7u, 7u, false, false));
    assert(presenterStateNeedsBind(true, 8u, 7u, false, false));
    assert(presenterStateNeedsBind(true, 7u, 7u, true, false));

    return 0;
}
