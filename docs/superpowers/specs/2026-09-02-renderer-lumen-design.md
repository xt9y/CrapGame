# CrapGame Renderer + Lumen Architecture Design

## Scope

Implement a complete software-Lumen-style renderer in `xt9y/CrapGame` directly on `main` while preserving these permanent constraints:

- `lwcgl 2.9.3` remains the only rendering/window/input dependency.
- No Vulkan, SDL, GLM, Assimp, Eigen, or other rendering/math libraries.
- The rest of the program sees only one rendering entry point: `Renderer::Rendering`.
- ECS owns world description only.
- Rendering resources, algorithms, caches, passes, temporal history, shadows, GI, reflections, and RendererCheck debug outputs stay behind `Renderer::Rendering`.
- The current coding style is authoritative.

## Naming and style

Directories and source files use PascalCase.

```text
Sources/
├── main.cpp
├── Ecs/
│   ├── Ecs.hpp
│   └── Ecs.cpp
└── Renderer/
    ├── Render.hpp
    ├── Render.cpp
    ├── Math/
    │   ├── Math.hpp
    │   └── Math.cpp
    ├── Mesh/
    │   ├── Mesh.hpp
    │   └── Mesh.cpp
    ├── Shader/
    │   ├── Shader.hpp
    │   └── Shader.cpp
    ├── GBuffer/
    │   ├── GBuffer.hpp
    │   └── GBuffer.cpp
    ├── Lighting/
    │   ├── Lighting.hpp
    │   └── Lighting.cpp
    ├── Shadows/
    │   ├── Shadows.hpp
    │   └── Shadows.cpp
    ├── Temporal/
    │   ├── Temporal.hpp
    │   └── Temporal.cpp
    ├── Lumen/
    │   ├── Lumen.hpp
    │   ├── Lumen.cpp
    │   ├── ScreenTrace.hpp
    │   ├── ScreenTrace.cpp
    │   ├── DistanceField.hpp
    │   ├── DistanceField.cpp
    │   ├── SurfaceCache.hpp
    │   ├── SurfaceCache.cpp
    │   ├── RadianceCache.hpp
    │   ├── RadianceCache.cpp
    │   ├── ScreenProbe.hpp
    │   ├── ScreenProbe.cpp
    │   ├── Radiosity.hpp
    │   ├── Radiosity.cpp
    │   ├── Reflections.hpp
    │   ├── Reflections.cpp
    │   ├── ShortRangeAo.hpp
    │   └── ShortRangeAo.cpp
    └── Test/
        ├── TestScene.hpp
        └── TestScene.cpp
```

Function names use camelCase and never contain underscores.

Variable names use snake_case only.

Same-type variables declared together should be grouped with commas where this improves readability.

Long function declarations/calls are wrapped in the existing project style, for example:

```cpp
void functionDoesSomething (
                TypeOfSomething name_of_something,
                TypeOfSomethingElse *name_of_somethingelse
        );
```

Braces, spacing before parentheses, separated `if` statements, vertically aligned declarations, and multiline long expressions follow the current `main.cpp` style exactly.

## Public architecture

Outside code sees:

```cpp
namespace Ecs
{
    class World;
}

namespace Renderer
{
    class Rendering
    {
    public:
        bool init ();
        void resize (int width, int height);
        void render (const Ecs::World& world);
        void prepareTestScene (Ecs::World& world);
        void shutdown ();
    };
}
```

`main.cpp` must remain thin. It owns application lifetime, creates the normal ECS scene, updates gameplay transforms, and calls `Rendering`. It does not know about render passes, shadow maps, GBuffer attachments, SDFs, cards, probes, caches, GI, reflections, or temporal history.

## ECS architecture

Rename `Sources/ECS` to `Sources/Ecs` and `ecs.hpp/.cpp` to `Ecs.hpp/.cpp`. Rename namespace `ecs` to `Ecs`.

ECS contains world-facing components only:

- `TransformComponent`
- `CameraComponent`
- `MeshComponent`
- `MaterialComponent`
- `LightComponent`

`LightComponent` supports:

- directional lights
- point lights
- spot lights
- RGB color
- intensity
- range
- inner and outer cone
- shadow enable
- indirect-light intensity

Light position/orientation come from `TransformComponent`.

Renderer-internal structures such as GBuffer handles, shadow maps, SDFs, Cards, Surface Cache pages, radiance probes, screen probes, temporal history, and reflection data never become ECS components.

## Rendering architecture

`Renderer::Rendering::render()` is the orchestration boundary. Its internal pipeline is:

1. Synchronize changed ECS scene data.
2. Update GPU meshes/material/light buffers.
3. Geometry/GBuffer pass.
4. Shadow passes.
5. Direct PBR lighting.
6. Screen-space tracing.
7. Mesh/Global SDF tracing fallback.
8. Surface Cache and Card updates.
9. Lumen Scene direct-light cache updates.
10. World-space Radiance Cache updates.
11. Screen Probe tracing and gather.
12. Radiosity/multi-bounce update.
13. Short-range AO/contact GI.
14. Lumen rough and smooth reflections.
15. Temporal reconstruction/filtering.
16. Final composition.
17. RendererCheck capture/debug output when requested.

All GPU work remains through APIs exposed by `lwcgl 2.9.3`.

## Renderer stages

### Foundation

1. Rename folders/files/namespaces/classes.
2. Implement own `Vec2`, `Vec3`, `Vec4`, `Mat4` math.
3. Replace fixed-function matrix construction with renderer-owned model/view/projection matrices.
4. Add indexed mesh representation.
5. Add VBO/IBO/VAO GPU mesh storage.
6. Add GLSL shader compilation/linking/uniform management.
7. Add ECS material data.
8. Complete ECS light data.

### Normal renderer

9. GBuffer/deferred geometry pass.
10. PBR BRDF: Lambert diffuse, GGX distribution, Smith visibility, Schlick Fresnel.
11. Direct point/directional/spot ECS lighting.
12. Point/directional/spot shadow maps.

### Temporal foundation

13. Previous-frame object/camera state.
14. Motion vectors.
15. Temporal history and reprojection validation.
16. TAA.

### Lumen tracing

17. Screen-space ray tracing.
18. Mesh signed-distance-field generation/storage.
19. SDF sphere tracing.
20. Global SDF clipmaps.
21. Unified screen-trace then SDF fallback path.

### Lumen scene/cache

22. Mesh Cards.
23. Surface Cache material atlas.
24. ECS lights into Surface Cache direct-light data.
25. World-space Radiance Cache.
26. Screen Probe placement/tracing/gather.
27. Importance sampling for probe rays.
28. Temporal GI accumulation.
29. Cached radiosity/multiple-bounce feedback.
30. Short-range AO/contact GI.

### Reflections and production behavior

31. Rough Lumen reflections using cached radiance.
32. Smooth traced reflections.
33. Temporal/spatial reflection filtering.
34. Dirty-region tracking from ECS transforms/materials/lights.
35. Per-frame update budgets for SDF, Surface Cache, radiance probes, and related work.
36. RendererCheck/debug visualization suite.

## Permanent normal scene

The normal application scene remains constant through the entire renderer build:

- one perspective ECS camera
- one white cube
- cube tilted so a body diagonal points vertically/downward like a diamond
- cube rotates about world Y
- one large ground plane
- one ECS point light above/off-axis from the cube

The same scene progresses from unlit rendering to PBR, shadows, GI, multi-bounce light, AO, and reflections.

## Software-Lumen target

This renderer targets the software ray-tracing architecture that is compatible with the permanent lwcgl/OpenGL constraint:

- screen-space traces first
- mesh SDF fallback
- global SDF for world tracing
- Surface Cache/Cards
- world-space radiance probes
- screen probes
- temporal accumulation
- radiosity feedback
- rough/smooth reflections

No hardware RT API is introduced.

## RendererCheck architecture

RendererCheck becomes a first-class renderer validation system.

The project keeps one executable. RendererCheck selects deterministic validation scenes using `RENDERCHECK_TEST`. `Rendering::prepareTestScene()` maps that test name to an internal test-scene builder.

RendererCheck tests use the real ECS and real renderer pipeline. No separate fake renderer is allowed.

The renderer supports internal debug output modes for intermediate buffers and passes, including:

- final
- albedo
- normal
- depth
- material data
- motion vectors
- direct lighting
- shadows
- mesh SDF
- global SDF
- Surface Cache
- Surface Cache lighting
- radiance cache
- screen probes
- indirect lighting
- short-range AO
- reflections

## RendererCheck visual test matrix

The final suite includes deterministic tests covering at least:

### Geometry and camera

- `GeometryCube`
- `GeometryPlane`
- `Depth`
- `CameraPerspective`
- `Normals`

### Materials and GBuffer

- `MaterialAlbedo`
- `MaterialRoughness`
- `MaterialMetallic`
- `MaterialEmissive`
- `GBufferAlbedo`
- `GBufferNormal`
- `GBufferDepth`
- `GBufferMaterial`

### Direct lighting and PBR

- `DirectPoint`
- `DirectDirectional`
- `DirectSpot`
- `LightFalloff`
- `LightColor`
- `PbrDiffuse`
- `PbrSpecular`
- `PbrFresnel`

### Shadows

- `ShadowPoint`
- `ShadowDirectional`
- `ShadowSpot`
- `ShadowBias`

### Temporal

- `MotionVectors`
- `TemporalStatic`
- `TemporalMotion`
- `TaaEdges`

### Tracing and distance fields

- `ScreenTraceHit`
- `ScreenTraceMiss`
- `MeshSdfCube`
- `MeshSdfPlane`
- `SdfInsideOutside`
- `SdfTraceHit`
- `SdfTraceMiss`
- `GlobalSdf`
- `LumenTraceFallback`

### Surface Cache

- `LumenCardCoverage`
- `SurfaceCacheAlbedo`
- `SurfaceCacheNormal`
- `SurfaceCacheLighting`

### Radiance cache and probes

- `RadianceProbe`
- `RadianceCache`
- `ScreenProbePlacement`
- `ScreenProbeTrace`
- `ScreenProbeGather`

### GI

- `LumenDirectOnly`
- `LumenIndirectOnly`
- `LumenBounceOne`
- `LumenMultiBounce`
- `LumenColorBleed`
- `LumenOcclusion`
- `ShortRangeAo`
- `EmissiveGi`

### Reflections

- `ReflectionRough`
- `ReflectionSmooth`
- `ReflectionOffscreen`
- `ReflectionTemporal`

### Dynamic invalidation and convergence

- `MovingLightGi`
- `MovingObjectGi`
- `SurfaceCacheDirty`
- `RadianceCacheDirty`
- `LumenConvergence`
- `LumenStress`
- `LumenFinal`
- `FinalScene`

Tests which need temporal convergence use larger RendererCheck `warmup_frames` values. Direct/static tests use short warmups. Intermediate-buffer tests capture the relevant renderer debug output instead of relying only on final-image comparisons.

## RendererCheck validation principles

- Every major renderer subsystem gets at least one isolated visual regression test.
- Every fallback path gets a forced-miss/forced-hit test where feasible.
- Lumen off-screen tracing must be validated using geometry absent from the camera view but visible in reflections/GI through SDF tracing.
- GI tests use intentionally simple color-bleed/emissive arrangements so failures are visually obvious.
- Shadow tests isolate bias, falloff, and light type behavior.
- Temporal tests use deterministic frame counts and motion.
- Final-scene tests do not replace isolated tests.
- New renderer stages are not considered complete until their RendererCheck scenes and baselines can be produced.

## Error handling

Initialization returns failure when required lwcgl/OpenGL capabilities or renderer resources cannot be created. Rendering skips only the unavailable pass when a recoverable resource fails; unrecoverable core initialization errors abort cleanly. Shader compile/link errors print complete renderer diagnostics.

RendererCheck scene selection fails explicitly for an unknown test name rather than silently running the default scene.

## Success criteria

The architecture is complete when:

- normal execution shows the permanent cube/ground/light scene through the full renderer
- ECS lights drive direct lighting, shadows, cached scene lighting, GI, and reflections
- moving lights/objects invalidate only affected renderer caches
- direct and indirect lighting are independently visualizable
- screen traces correctly fall back to software SDF traces
- Surface Cache, Radiance Cache, Screen Probe Gather, radiosity, AO, and reflection passes are active
- temporal GI/reflections converge deterministically
- all renderer functionality remains behind `Renderer::Rendering`
- no dependency beyond the existing project stack is introduced
- the full RendererCheck suite can independently validate all major stages
- all new code follows the current CrapGame coding style and naming rules
