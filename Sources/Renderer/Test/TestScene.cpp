#include "TestScene.hpp"
#include "Models/Models.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace Renderer
{
namespace Test
{
namespace
{

bool isTest (const char *test_name, const char *expected)
{
    return test_name && expected && std::strcmp(test_name, expected) == 0;
}

Ecs::Entity addCamera (Ecs::World *world)
{
    const Ecs::Entity camera = world->createEntity();
    world->addTransform(camera, {{0.0f, 3.0f, 8.0f}, {-12.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    world->addCamera(camera, {60.0f, 0.1f, 100.0f, true});
    return camera;
}

Ecs::Entity addCube (Ecs::World *world, const Ecs::Vec3& position, const Ecs::Vec3& rotation, const Ecs::Vec3& scale, const Ecs::Vec3& albedo, float metallic, float roughness)
{
    const Ecs::Entity cube = world->createEntity();
    world->addTransform(cube, {position, rotation, scale});
    world->addMesh(cube, {Ecs::MeshType::Cube});
    world->addRenderable(cube, {true});
    world->addMaterial(cube, {albedo, {0.0f, 0.0f, 0.0f}, metallic, roughness, 0.0f});
    return cube;
}

Ecs::Entity addPlane (Ecs::World *world, const Ecs::Vec3& position, const Ecs::Vec3& rotation, const Ecs::Vec3& scale, const Ecs::Vec3& albedo, float metallic, float roughness)
{
    const Ecs::Entity plane = world->createEntity();
    world->addTransform(plane, {position, rotation, scale});
    world->addMesh(plane, {Ecs::MeshType::Plane});
    world->addRenderable(plane, {true});
    world->addMaterial(plane, {albedo, {0.0f, 0.0f, 0.0f}, metallic, roughness, 0.0f});
    return plane;
}

Ecs::Entity addLight (Ecs::World *world, Ecs::LightType type, const Ecs::Vec3& position, const Ecs::Vec3& rotation, const Ecs::Vec3& color, float intensity, float range, bool casts_shadows)
{
    const Ecs::Entity light = world->createEntity();
    world->addTransform(light, {position, rotation, {1.0f, 1.0f, 1.0f}});
    world->addLight(light, {type, color, intensity, range, 20.0f, 35.0f, 1.0f, casts_shadows});
    return light;
}

void configureDirectional (Ecs::World *world, Ecs::Entity light)
{
    Ecs::LightComponent *component = world->getLight(light);
    Ecs::TransformComponent *transform = world->getTransform(light);
    if (component)
    {
        component->type = Ecs::LightType::Directional;
        component->intensity = 1.8f;
        component->range = 100.0f;
    }
    if (transform)
    {
        transform->position = {0.0f, 6.0f, 0.0f};
        transform->rotation = {-55.0f, -135.0f, 0.0f};
    }
}

void configureSpot (Ecs::World *world, Ecs::Entity light)
{
    Ecs::LightComponent *component = world->getLight(light);
    Ecs::TransformComponent *transform = world->getTransform(light);
    if (component)
    {
        component->type = Ecs::LightType::Spot;
        component->intensity = 120.0f;
        component->range = 18.0f;
        component->inner_cone = 18.0f;
        component->outer_cone = 32.0f;
    }
    if (transform)
    {
        transform->position = {3.5f, 6.0f, 4.0f};
        transform->rotation = {-48.0f, -150.0f, 0.0f};
    }
}

void addColorBleedScene (Ecs::World *world)
{
    addPlane(world, {-2.2f, 1.8f, 0.0f}, {0.0f, 0.0f, -90.0f}, {3.6f, 1.0f, 3.6f}, {0.95f, 0.04f, 0.03f}, 0.0f, 0.75f);
}

void addOcclusionScene (Ecs::World *world)
{
    addCube(world, {-1.8f, 1.0f, 0.6f}, {0.0f, 25.0f, 0.0f}, {1.2f, 2.0f, 1.2f}, {0.38f, 0.42f, 0.50f}, 0.0f, 0.65f);
}

void addStressScene (Ecs::World *world)
{
    for (int z = 0; z < 3; ++z)
    {
        for (int x = 0; x < 5; ++x)
        {
            const float position_x = static_cast<float>(x - 2) * 1.8f;
            const float position_z = -2.0f - static_cast<float>(z) * 1.8f;
            addCube(world, {position_x, 0.75f, position_z}, {static_cast<float>(z * 12), static_cast<float>(x * 17), 0.0f}, {0.65f, 0.65f, 0.65f}, {0.20f + 0.12f * static_cast<float>(x), 0.25f + 0.10f * static_cast<float>(z), 0.55f}, x % 2 == 0 ? 0.65f : 0.0f, 0.25f + 0.12f * static_cast<float>(z));
        }
    }
    addLight(world, Ecs::LightType::Point, {-4.0f, 4.5f, -2.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 0.15f, 0.08f}, 55.0f, 12.0f, true);
    addLight(world, Ecs::LightType::Point, {4.0f, 4.0f, -4.0f}, {0.0f, 0.0f, 0.0f}, {0.10f, 0.35f, 1.0f}, 60.0f, 12.0f, true);
}

const char *KNOWN_TESTS[] = {
    "GeometryCube", "GeometryPlane", "Depth", "CameraPerspective", "Normals",
    "MaterialAlbedo", "MaterialRoughness", "MaterialMetallic", "MaterialEmissive",
    "GBufferAlbedo", "GBufferNormal", "GBufferDepth", "GBufferMaterial",
    "DirectPoint", "DirectDirectional", "DirectSpot", "LightFalloff", "LightColor",
    "PbrDiffuse", "PbrSpecular", "PbrFresnel", "ShadowPoint", "ShadowDirectional",
    "ShadowSpot", "ShadowBias", "MotionVectors", "TemporalStatic", "TemporalMotion",
    "TaaEdges", "ScreenTraceHit", "ScreenTraceMiss", "MeshSdfCube", "MeshSdfPlane",
    "SdfInsideOutside", "SdfTraceHit", "SdfTraceMiss", "GlobalSdf", "LumenTraceFallback",
    "LumenCardCoverage", "SurfaceCacheAlbedo", "SurfaceCacheNormal", "SurfaceCacheLighting",
    "RadianceProbe", "RadianceCache", "ScreenProbePlacement", "ScreenProbeTrace",
    "ScreenProbeGather", "LumenDirectOnly", "LumenIndirectOnly", "LumenBounceOne",
    "LumenMultiBounce", "LumenColorBleed", "LumenOcclusion", "ShortRangeAo",
    "ReflectionRough", "ReflectionSmooth", "ReflectionOffscreen", "ReflectionTemporal",
    "EmissiveGi", "MovingLightGi", "MovingObjectGi", "SurfaceCacheDirty",
    "RadianceCacheDirty", "LumenConvergence", "LumenFinal", "LumenStress", "FinalScene",
};

} // namespace

bool knownTest (const char *test_name)
{
    if (!test_name || !*test_name) return true;
    for (const char *known_test : KNOWN_TESTS) if (isTest(test_name, known_test)) return true;
    return false;
}

bool buildScene (const char *test_name, Ecs::World *world, SceneState *state)
{
    if (!world || !state || !knownTest(test_name)) return false;

    state->test_name = test_name && *test_name ? test_name : "FinalScene";
    state->renderercheck = test_name && *test_name;

    const Ecs::Entity camera = addCamera(world);

    if (!state->renderercheck)
    {
        if (Ecs::TransformComponent *camera_transform = world->getTransform(camera))
        {
            camera_transform->position = {0.0f, 2.5f, 8.0f};
            camera_transform->rotation = {0.0f, 0.0f, 0.0f};
        }
        if (Ecs::CameraComponent *camera_component = world->getCamera(camera))
        {
            camera_component->near_plane = 0.05f;
            camera_component->far_plane = 100.0f;
        }

        Models::SpawnOptions options;
        options.transform.scale = {0.01f, 0.01f, 0.01f};
        std::string error;
        const std::vector<Ecs::Entity> sponza = Models::loadInto(*world, "Assets/Sponza/sponza.obj", options, &error);
        if (sponza.empty())
        {
            std::fprintf(stderr, "Sponza load failed: %s\n", error.empty() ? "no renderable submeshes" : error.c_str());
            return false;
        }

        state->cube = Ecs::INVALID_ENTITY;
        state->ground = Ecs::INVALID_ENTITY;
        state->light = addLight(world, Ecs::LightType::Directional, {0.0f, 8.0f, 0.0f}, {-55.0f, -135.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 3.0f, 100.0f, true);
        return true;
    }

    state->cube = addCube(world, {0.0f, 1.45f, 0.0f}, {-35.2643897f, 0.0f, 45.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, 0.0f, 0.35f);
    state->ground = addPlane(world, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {14.0f, 1.0f, 14.0f}, {0.28f, 0.30f, 0.34f}, 0.0f, 0.80f);
    state->light = addLight(world, Ecs::LightType::Point, {3.0f, 5.0f, 2.0f}, {-45.0f, -135.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 40.0f, 15.0f, true);

    if (isTest(state->test_name, "GeometryPlane") || isTest(state->test_name, "MeshSdfPlane"))
    {
        Ecs::RenderableComponent *cube = world->getRenderable(state->cube);
        if (cube) cube->visible = false;
    }
    if (isTest(state->test_name, "MaterialRoughness"))
    {
        Ecs::MaterialComponent *material = world->getMaterial(state->cube);
        if (material) material->roughness = 0.95f;
    }
    if (isTest(state->test_name, "MaterialMetallic") || isTest(state->test_name, "PbrSpecular") || isTest(state->test_name, "PbrFresnel"))
    {
        Ecs::MaterialComponent *material = world->getMaterial(state->cube);
        if (material) { material->metallic = 1.0f; material->roughness = 0.08f; }
    }
    if (isTest(state->test_name, "MaterialAlbedo"))
    {
        Ecs::MaterialComponent *material = world->getMaterial(state->cube);
        if (material) material->albedo = {0.08f, 0.55f, 0.95f};
    }
    if (isTest(state->test_name, "MaterialEmissive") || isTest(state->test_name, "EmissiveGi"))
    {
        Ecs::MaterialComponent *material = world->getMaterial(state->cube);
        if (material) { material->albedo = {0.15f, 0.05f, 0.02f}; material->emissive = {1.0f, 0.22f, 0.04f}; material->emissive_strength = 4.0f; }
    }
    if (isTest(state->test_name, "EmissiveGi"))
    {
        Ecs::LightComponent *light = world->getLight(state->light);
        if (light) light->intensity = 0.0f;
    }
    if (isTest(state->test_name, "PbrDiffuse"))
    {
        Ecs::MaterialComponent *material = world->getMaterial(state->cube);
        if (material) { material->metallic = 0.0f; material->roughness = 1.0f; }
    }
    if (isTest(state->test_name, "DirectDirectional") || isTest(state->test_name, "ShadowDirectional")) configureDirectional(world, state->light);
    if (isTest(state->test_name, "DirectSpot") || isTest(state->test_name, "ShadowSpot")) configureSpot(world, state->light);
    if (isTest(state->test_name, "LightColor"))
    {
        Ecs::LightComponent *light = world->getLight(state->light);
        if (light) light->color = {1.0f, 0.10f, 0.04f};
    }
    if (isTest(state->test_name, "LightFalloff")) state->secondary = addCube(world, {3.8f, 1.0f, -1.5f}, {0.0f, 25.0f, 0.0f}, {0.8f, 0.8f, 0.8f}, {1.0f, 1.0f, 1.0f}, 0.0f, 0.55f);
    if (isTest(state->test_name, "ShadowBias"))
    {
        Ecs::TransformComponent *transform = world->getTransform(state->cube);
        if (transform) transform->position.y = 0.95f;
    }
    if (isTest(state->test_name, "LumenColorBleed")) addColorBleedScene(world);
    if (isTest(state->test_name, "LumenOcclusion") || isTest(state->test_name, "ShortRangeAo")) addOcclusionScene(world);
    if (isTest(state->test_name, "ReflectionRough"))
    {
        Ecs::MaterialComponent *material = world->getMaterial(state->ground);
        if (material) { material->metallic = 0.85f; material->roughness = 0.65f; }
    }
    if (isTest(state->test_name, "ReflectionSmooth") || isTest(state->test_name, "ReflectionTemporal") || isTest(state->test_name, "ReflectionOffscreen"))
    {
        Ecs::MaterialComponent *material = world->getMaterial(state->ground);
        if (material) { material->metallic = 1.0f; material->roughness = 0.06f; }
    }
    if (isTest(state->test_name, "ReflectionOffscreen")) state->secondary = addCube(world, {5.2f, 1.2f, -0.5f}, {0.0f, 25.0f, 0.0f}, {1.2f, 1.2f, 1.2f}, {0.10f, 0.35f, 1.0f}, 0.65f, 0.12f);
    if (isTest(state->test_name, "LumenStress")) addStressScene(world);
    return true;
}

void updateScene (Ecs::World *world, SceneState *state, std::uint64_t frame)
{
    if (!world || !state) return;
    Ecs::TransformComponent *cube = world->getTransform(state->cube);
    Ecs::TransformComponent *light = world->getTransform(state->light);
    const float time = static_cast<float>(frame);

    if (!state->renderercheck || isTest(state->test_name, "FinalScene") || isTest(state->test_name, "LumenFinal"))
    {
        if (cube)
        {
            cube->rotation.y += 0.65f;
            if (cube->rotation.y >= 360.0f) cube->rotation.y -= 360.0f;
        }
    }
    if (isTest(state->test_name, "MotionVectors") || isTest(state->test_name, "TemporalMotion") || isTest(state->test_name, "MovingObjectGi") || isTest(state->test_name, "SurfaceCacheDirty"))
    {
        if (cube) { cube->position.x = std::sin(time * 0.22f) * 1.1f; cube->rotation.y = time * 4.0f; }
    }
    if (isTest(state->test_name, "MovingLightGi") || isTest(state->test_name, "RadianceCacheDirty"))
    {
        if (light) { light->position.x = std::sin(time * 0.18f) * 3.5f; light->position.z = 2.0f + std::cos(time * 0.18f) * 1.5f; }
    }
    if (isTest(state->test_name, "ReflectionTemporal"))
    {
        if (cube) { cube->position.z = std::sin(time * 0.16f) * 1.6f; cube->rotation.y = time * 3.0f; }
    }
}

} // namespace Test
} // namespace Renderer
