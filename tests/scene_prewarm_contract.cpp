#include "Renderer/Gpu/ScenePrewarm.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

static void require (bool value, const char *message)
{
    if (!value)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

struct FakeGBuffer
{
    int resize_calls = 0;
    int prewarm_calls = 0;

    bool resize (int width, int height, std::string*)
    {
        ++resize_calls;
        return width == 1280 && height == 870;
    }

    bool prewarm (const Ecs::World&, std::string*)
    {
        ++prewarm_calls;
        return true;
    }
};

struct FakeDirect : FakeGBuffer
{
};

struct FakeLumen
{
    int resize_calls = 0;
    int trace_prewarm_calls = 0;

    bool resize (int width, int height, std::string*)
    {
        ++resize_calls;
        return width == 1280 && height == 870;
    }

    bool prewarmImportedTrace (std::string*)
    {
        ++trace_prewarm_calls;
        return true;
    }
};

int main ()
{
    Ecs::World world;
    FakeGBuffer gbuffer;
    FakeDirect direct;
    FakeLumen lumen;
    Renderer::Gpu::ScenePrewarm prewarm;
    std::string error = "stale";

    require(
        prewarm.runWith(world, gbuffer, direct, lumen, 1280, 870, &error),
        "prewarm succeeds when all owners become resident"
    );
    require(prewarm.complete(), "prewarm completion is recorded");
    require(error.empty(), "successful prewarm clears the error");
    require(gbuffer.resize_calls == 1 && gbuffer.prewarm_calls == 1,
            "GBuffer storage and material residency are prewarmed once");
    require(direct.resize_calls == 1 && direct.prewarm_calls == 1,
            "direct-light targets and imported acceleration are prewarmed once");
    require(lumen.resize_calls == 1 && lumen.trace_prewarm_calls == 1,
            "Lumen targets and imported trace shader are prewarmed once");

    prewarm.reset();
    require(!prewarm.complete(), "reset clears completion state");

    std::cout << "scene_prewarm_contract=PASS\n";
    return 0;
}
