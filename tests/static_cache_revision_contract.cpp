#include "Renderer/Gpu/RevisionState.hpp"

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
    using namespace Renderer::Gpu;

    RevisionState revisions = {};
    RevisionState camera = revisions;
    ++camera.camera;

    require(!sameFrameInputs(revisions, camera),
            "camera changes final-frame inputs");
    require(staticShadowValid(revisions, camera),
            "camera does not invalidate static shadows");
    require(worldRadianceValid(revisions, camera),
            "camera does not invalidate radiance cache");

    RevisionState light = revisions;
    ++light.lighting;
    require(!staticShadowValid(revisions, light),
            "light invalidates static shadows");
    require(!worldRadianceValid(revisions, light),
            "light invalidates radiance cache");

    RevisionState resolution = revisions;
    ++resolution.resolution;
    require(staticShadowValid(revisions, resolution),
            "resize does not invalidate world shadow contents");
    require(worldRadianceValid(revisions, resolution),
            "resize does not invalidate world radiance");

    RevisionState state = {};
    applyRevisionChanges(&state, true, true, true, true, 7u, 9u);
    require(state.geometry == 1u
            && state.material == 1u
            && state.lighting == 1u
            && state.camera == 1u,
            "semantic revisions advance independently");
    require(state.mesh_registry == 7u && state.material_registry == 9u,
            "registry revisions are captured");

    std::cout << "static_cache_revision_contract=PASS\n";
    return 0;
}
