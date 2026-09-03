#include "Renderer/GBuffer/GBuffer.hpp"
#include "Renderer/Lumen/ScreenTrace.hpp"
#include "Renderer/Lumen/SphereTrace.hpp"
#include "Renderer/Math/Math.hpp"

#include <array>
#include <cassert>

namespace
{

void addRenderableCube(Ecs::World *world, const Ecs::TransformComponent& transform)
{
    const Ecs::Entity entity = world->createEntity();
    world->addTransform(entity, transform);
    world->addMesh(entity, {Ecs::MeshType::Cube});
    world->addRenderable(entity, {true});
}

void screenDistanceMatchesFullTrace()
{
    Renderer::GBuffer::Buffer gbuffer;
    gbuffer.resize(8, 8);

    Renderer::GBuffer::Pixel& pixel = gbuffer.pixel(4, 4);
    pixel.world_position = {0.0f, 0.0f, 0.12f};
    pixel.normal = {0.0f, 0.0f, -1.0f};
    pixel.entity = 7u;
    pixel.valid = true;

    const Renderer::Math::Mat4 identity = Renderer::Math::identity();
    const Renderer::Math::Vec3 origin = {0.0f, 0.0f, 0.0f};
    const Renderer::Math::Vec3 direction = {0.0f, 0.0f, 1.0f};

    const Renderer::Lumen::TraceHit full =
        Renderer::Lumen::traceScreenNormalized(
                gbuffer,
                identity,
                identity,
                origin,
                direction,
                0.8f,
                0.12f,
                0.18f
            );

    const Renderer::Lumen::ScreenDistanceHit distance_only =
        Renderer::Lumen::traceScreenDistanceNormalized(
                gbuffer,
                identity,
                identity,
                origin,
                direction,
                0.8f,
                0.12f,
                0.18f
            );

    assert(full.hit == distance_only.hit);
    assert(full.distance == distance_only.distance);

    gbuffer.clear();

    const Renderer::Lumen::TraceHit full_miss =
        Renderer::Lumen::traceScreenNormalized(
                gbuffer,
                identity,
                identity,
                origin,
                direction,
                0.8f,
                0.12f,
                0.18f
            );

    const Renderer::Lumen::ScreenDistanceHit distance_miss =
        Renderer::Lumen::traceScreenDistanceNormalized(
                gbuffer,
                identity,
                identity,
                origin,
                direction,
                0.8f,
                0.12f,
                0.18f
            );

    assert(full_miss.hit == distance_miss.hit);
    assert(full_miss.distance == distance_miss.distance);
}

void sdfDistanceOnlyMatchesGenericDistance()
{
    Ecs::World world;

    addRenderableCube(&world, {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f},
    });

    addRenderableCube(&world, {
        {2.4f, 0.7f, -1.6f},
        {17.0f, 31.0f, 9.0f},
        {0.65f, 1.4f, 0.9f},
    });

    Renderer::Lumen::DistanceFieldScene scene;
    scene.build(world);

    const std::array<Renderer::Math::Vec3, 8> positions = {{
        {0.0f, 0.0f, 0.0f},
        {0.6f, 0.2f, 0.1f},
        {1.2f, 0.0f, 0.0f},
        {2.4f, 0.7f, -1.6f},
        {2.9f, 1.1f, -1.2f},
        {-3.0f, 2.0f, 1.0f},
        {5.0f, -1.0f, -4.0f},
        {0.2f, 3.5f, -0.7f},
    }};

    for (const Renderer::Math::Vec3& position : positions)
    {
        const float generic = scene.distance(position, nullptr);
        const float distance_only = scene.distanceOnly(position);
        assert(generic == distance_only);
    }
}

} // namespace

int main()
{
    screenDistanceMatchesFullTrace();
    sdfDistanceOnlyMatchesGenericDistance();
    return 0;
}
