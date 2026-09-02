#ifndef CRAPGAME_RENDERER_TEST_TESTSCENE_HPP
#define CRAPGAME_RENDERER_TEST_TESTSCENE_HPP

#include "Ecs/Ecs.hpp"

#include <cstdint>

namespace Renderer
{
namespace Test
{

struct SceneState
{
    Ecs::Entity cube      = Ecs::INVALID_ENTITY,
                ground    = Ecs::INVALID_ENTITY,
                light     = Ecs::INVALID_ENTITY,
                secondary = Ecs::INVALID_ENTITY;

    const char *test_name = nullptr;
    bool renderercheck = false;
};

bool knownTest (const char *test_name);

bool buildScene (
                const char *test_name,
                Ecs::World *world,
                SceneState *state
        );

void updateScene (
                Ecs::World *world,
                SceneState *state,
                std::uint64_t frame
        );

} // namespace Test
} // namespace Renderer

#endif
