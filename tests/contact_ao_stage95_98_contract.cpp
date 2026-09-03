#include "Renderer/GBuffer/GBuffer.hpp"
#include "Renderer/Lumen/ScreenTrace.hpp"
#include "Renderer/Lumen/SphereTrace.hpp"
#include "Renderer/Lumen/TrilinearIndex.hpp"
#include "Renderer/Math/Math.hpp"

#include <array>
#include <cassert>
#include <cstddef>

namespace
{

void addRenderableCube(Ecs::World *world, const Ecs::TransformComponent& transform)
{
    const Ecs::Entity entity = world->createEntity();
    world->addTransform(entity, transform);
    world->addMesh(entity, {Ecs::MeshType::Cube});
    world->addRenderable(entity, {true});
}

std::size_t legacyIndex(int x, int y, int z, int resolution)
{
    return static_cast<std::size_t>(z) *
               static_cast<std::size_t>(resolution) *
               static_cast<std::size_t>(resolution) +
           static_cast<std::size_t>(y) *
               static_cast<std::size_t>(resolution) +
           static_cast<std::size_t>(x);
}

void exactTrilinearIndicesMatchLegacy()
{
    struct IndexCase
    {
        int x0, x1,
            y0, y1,
            z0, z1,
            resolution;
    };

    constexpr IndexCase cases[] = {
        {0, 1, 0, 1, 0, 1, 4},
        {2, 3, 5, 6, 7, 8, 18},
        {26, 27, 26, 27, 26, 27, 28},
        {17, 17, 17, 17, 17, 17, 18},
        {0, 0, 27, 27, 12, 13, 28},
    };

    for (const IndexCase& test : cases)
    {
        const std::size_t resolution_squared =
            static_cast<std::size_t>(test.resolution) *
            static_cast<std::size_t>(test.resolution);

        const Renderer::Lumen::TrilinearIndices indices =
            Renderer::Lumen::trilinearIndicesExact(
                    test.x0, test.x1,
                    test.y0, test.y1,
                    test.z0, test.z1,
                    test.resolution,
                    resolution_squared
                );

        assert(indices.x0y0z0 == legacyIndex(test.x0, test.y0, test.z0, test.resolution));
        assert(indices.x1y0z0 == legacyIndex(test.x1, test.y0, test.z0, test.resolution));
        assert(indices.x0y1z0 == legacyIndex(test.x0, test.y1, test.z0, test.resolution));
        assert(indices.x1y1z0 == legacyIndex(test.x1, test.y1, test.z0, test.resolution));
        assert(indices.x0y0z1 == legacyIndex(test.x0, test.y0, test.z1, test.resolution));
        assert(indices.x1y0z1 == legacyIndex(test.x1, test.y0, test.z1, test.resolution));
        assert(indices.x0y1z1 == legacyIndex(test.x0, test.y1, test.z1, test.resolution));
        assert(indices.x1y1z1 == legacyIndex(test.x1, test.y1, test.z1, test.resolution));
    }
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
    exactTrilinearIndicesMatchLegacy();
    screenDistanceMatchesFullTrace();
    sdfDistanceOnlyMatchesGenericDistance();
    return 0;
}
