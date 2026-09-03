#include "Ecs/Ecs.hpp"
#include "Renderer/Lumen/Cards.hpp"
#include "Renderer/Lumen/SurfaceCache.hpp"
#include "Renderer/Math/Math.hpp"

#include <cassert>
#include <cstddef>
#include <limits>
#include <vector>

namespace
{

Renderer::Math::Vec3 legacyPreviousIndirect (
            const std::vector<Renderer::Lumen::SurfaceSample>& previous,
            const Renderer::Lumen::Card& card
    )
{
    Renderer::Math::Vec3 result = {0.0f, 0.0f, 0.0f};
    float best_score = std::numeric_limits<float>::max();

    for (const Renderer::Lumen::SurfaceSample& sample : previous)
    {
        if (sample.card.entity != card.entity)
        {
            continue;
        }

        const float facing = Renderer::Math::dot(
                sample.card.normal,
                card.normal
            );

        if (facing < 0.75f)
        {
            continue;
        }

        const float distance = Renderer::Math::lengthSquared(
                Renderer::Math::subtract(
                        sample.card.position,
                        card.position
                    )
            );

        const float score = distance + (1.0f - facing) * 0.5f;

        if (score < best_score)
        {
            best_score = score;
            result = sample.indirect_lighting;
        }
    }

    return result;
}

void addCube (Ecs::World *world, const Ecs::Vec3& position)
{
    const Ecs::Entity entity = world->createEntity();
    world->addTransform(entity, {
        position,
        {0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f},
    });
    world->addMesh(entity, {Ecs::MeshType::Cube});
    world->addRenderable(entity, {true});
    world->addMaterial(entity, {
        {0.4f, 0.5f, 0.6f},
        {0.0f, 0.0f, 0.0f},
        0.0f,
        0.5f,
        0.0f,
    });
}

} // namespace

int main ()
{
    Ecs::World world;
    addCube(&world, {-1.0f, 0.0f, 0.0f});
    addCube(&world, { 1.0f, 0.0f, 0.0f});

    Renderer::Lumen::CardScene cards;
    Renderer::Lumen::SurfaceCache cache;

    cards.build(world);
    cache.build(world, cards);
    assert(cache.samples().size() == 12u);

    for (std::size_t index = 0; index < cache.samples().size(); ++index)
    {
        const float value = static_cast<float>(index + 1u);
        cache.samples()[index].indirect_lighting = {
            value,
            value * 2.0f,
            value * 3.0f,
        };
    }

    const std::vector<Renderer::Lumen::SurfaceSample> previous =
        cache.samples();

    Ecs::TransformComponent *moved = world.getTransform(0u);
    assert(moved != nullptr);
    moved->position.x += 0.35f;
    moved->rotation.y = 17.0f;

    cards.build(world);
    cache.build(world, cards);
    assert(cache.samples().size() == 12u);

    for (const Renderer::Lumen::SurfaceSample& sample : cache.samples())
    {
        const Renderer::Math::Vec3 expected =
            legacyPreviousIndirect(previous, sample.card);

        assert(sample.indirect_lighting.x == expected.x);
        assert(sample.indirect_lighting.y == expected.y);
        assert(sample.indirect_lighting.z == expected.z);
    }

    return 0;
}
