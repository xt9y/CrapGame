#ifndef CRAPGAME_MODELS_MODELS_HPP
#define CRAPGAME_MODELS_MODELS_HPP

#include "Ecs/Ecs.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace Models
{

using ModelHandle = std::uint32_t;
constexpr ModelHandle INVALID_MODEL = UINT32_MAX;

struct SpawnOptions
{
    Ecs::TransformComponent transform = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f},
    };
    bool visible = true;
};

ModelHandle load (
                const std::string& path,
                std::string *error = nullptr
        );

std::vector<Ecs::Entity> spawn (
                Ecs::World& world,
                ModelHandle model,
                const SpawnOptions& options = {},
                std::string *error = nullptr
        );

std::vector<Ecs::Entity> loadInto (
                Ecs::World& world,
                const std::string& path,
                const SpawnOptions& options = {},
                std::string *error = nullptr
        );

void clearCache ();

} // namespace Models

#endif
