#include "Renderer/CpuReferencePolicy.hpp"

#include <cassert>

int main ()
{
    using Renderer::rendererRuntimePlan;

    const auto visual = rendererRuntimePlan(true, false);
    assert(!visual.display_required);
    assert(!visual.input_required);
    assert(!visual.window_updates_required);
    assert(visual.fixed_reference_size);

    const auto performance = rendererRuntimePlan(false, true);
    assert(performance.display_required);
    assert(performance.input_required);
    assert(performance.window_updates_required);
    assert(!performance.fixed_reference_size);

    const auto interactive = rendererRuntimePlan(false, false);
    assert(interactive.display_required);
    assert(interactive.input_required);
    assert(interactive.window_updates_required);
    assert(!interactive.fixed_reference_size);

    return 0;
}
