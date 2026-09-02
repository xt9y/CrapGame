#ifndef CRAPGAME_RENDERER_LUMEN_CARDS_HPP
#define CRAPGAME_RENDERER_LUMEN_CARDS_HPP

#include "Ecs/Ecs.hpp"
#include "Renderer/Math/Math.hpp"

#include <vector>

namespace Renderer 
{
namespace Lumen 
{

struct Card 
{
    Ecs::Entity entity;

    Math::Vec3 position,
               normal,
               tangent_u,
               tangent_v;

    float extent_u,
          extent_v;
};

class CardScene 
{

public:
    void build (const Ecs::World& world);
    const std::vector<Card>& cards () const;

private:
    std::vector<Card> cards_;
};

} // namespace Lumen
} // namespace Renderer

#endif
