# Models + Sponza Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a native OBJ/MTL `Sources/Models/` subsystem, loaded-mesh ECS/renderer support, and use `Assets/Sponza/sponza.obj` as the normal interactive scene.

**Architecture:** `Models` owns OBJ/MTL parsing and model caching. Loaded geometry is registered with `Renderer::Mesh` under stable integer handles; ECS stores those handles without owning renderer vectors. RendererCheck fixtures remain procedural, while the no-test scene loads Sponza.

**Tech Stack:** C++17, C-BuildSystem, existing ECS/renderer, OpenGL/lwcgl, no third-party model loader.

**Spec:** `docs/superpowers/specs/2026-09-04-models-design.md`

## Global Constraints

- The folder is `Sources/Models/`.
- No tinyobjloader, Assimp, or other model loader dependency.
- First format is OBJ + MTL.
- Preserve texture/material metadata even where the renderer does not sample it yet.
- Existing `{Ecs::MeshType::Cube}` and `{Ecs::MeshType::Plane}` source must remain valid.
- No file parsing or filesystem work in the frame hot path.
- `Assets/Sponza` is a git submodule pinned by the parent repository.
- `Assets/Sponza/sponza.obj` is the first normal interactive model.
- RendererCheck scenes must remain deterministic procedural fixtures.
- `build.c` must use generic `*.cpp` depth patterns rather than subsystem-specific source lines.

---

### Task 1: Sponza asset fixture

**Files:**
- Create: `.gitmodules`
- Add gitlink: `Assets/Sponza`

**Produces:** `Assets/Sponza/sponza.obj`, `Assets/Sponza/sponza.mtl`, and textures from `jimmiebergmann/Sponza`.

- [x] Add the submodule at `Assets/Sponza`.
- [x] Pin it to commit `222338979d32f4f4818466291bdbc29f192b86ba`.
- [ ] Verify `.gitmodules` and the `160000` gitlink from the main tree.

### Task 2: Parser contract first

**Files:**
- Create: `Sources/Models/Material.hpp`
- Create: `Sources/Models/Obj.hpp`
- Create: `Sources/Models/Obj.cpp`
- Create: `tests/models_obj_contract.cpp`

**Produces:** `Models::Internal::parseObj(const std::string&)` returning parsed submeshes/materials with descriptive exceptions.

- [ ] Write `tests/models_obj_contract.cpp` using temporary OBJ/MTL files. Cover positions-only triangles, UV/normals, negative indices, quads, generated normals, `usemtl`, multiple MTLs, `d`/`Tr`, PBR `Pr`/`Pm`, map paths/options, malformed indices, and missing MTL fallback.
- [ ] Compile/run the test before `Obj.cpp` exists and confirm the expected missing-symbol/API failure.
- [ ] Implement tokenizer/index resolution, deterministic fan triangulation, smoothing-aware normal generation, MTL parsing, map option parsing, and relative path resolution.
- [ ] Run the parser contract and require exit code 0.

### Task 3: Loaded mesh registry and ECS representation

**Files:**
- Modify: `Sources/Ecs/Ecs.hpp`
- Modify: `Sources/Renderer/Mesh/Mesh.hpp`
- Modify: `Sources/Renderer/Mesh/Mesh.cpp`
- Create: `tests/models_mesh_registry_contract.cpp`

**Interfaces:**
- `Ecs::MeshComponent { MeshType mesh; std::uint32_t loaded_mesh = UINT32_MAX; }`
- `Renderer::Mesh::LoadedMeshHandle registerLoadedMesh(MeshData mesh)`
- `const MeshData& meshForComponent(const Ecs::MeshComponent&)`
- `void clearLoadedMeshes()`

- [ ] Write a failing test proving procedural aggregate initialization still works and a registered custom triangle resolves through `meshForComponent`.
- [ ] Run it and confirm failure because the loaded-mesh API is absent.
- [ ] Implement stable registry handles and component resolution.
- [ ] Run the contract and existing Mesh/ECS compile checks.

### Task 4: Models public API and ECS spawning

**Files:**
- Create: `Sources/Models/Models.hpp`
- Create: `Sources/Models/Models.cpp`
- Create: `tests/models_spawn_contract.cpp`
- Modify: `Sources/Ecs/Ecs.hpp`

**Interfaces:**
- `using ModelHandle = std::uint32_t`
- `ModelHandle Models::load(const std::string& path)`
- `std::vector<Ecs::Entity> Models::spawn(Ecs::World&, ModelHandle, const SpawnOptions&)`
- `std::vector<Ecs::Entity> Models::loadInto(Ecs::World&, const std::string&, const SpawnOptions&)`
- `void Models::clearCache()`

- [ ] Write a failing spawn/cache test with a two-material OBJ.
- [ ] Confirm the test fails before the public API exists.
- [ ] Implement normalized-path cache, mesh registration, scalar material conversion, extended surface fields, material-asset handle preservation, spawn, loadInto, and clearCache.
- [ ] Run tests and confirm repeated loads return the same handle and spawning creates one render entity per submesh.

### Task 5: Renderer geometry resolution

**Files:**
- Modify: `Sources/Renderer/Render.cpp`
- Modify: `Sources/Renderer/Shadows/Shadows.cpp`
- Modify: `Sources/Renderer/Lumen/Cards.cpp`
- Modify: `Sources/Renderer/Lumen/SphereTrace.cpp`
- Create/modify contract tests under `tests/`.

- [ ] Add a failing source/runtime contract proving CPU rasterization, shadows, and card bounds use `Mesh::meshForComponent(*mesh)` rather than `meshForType(mesh->mesh)`.
- [ ] Replace geometry consumers with component resolution.
- [ ] For procedural-only signed-distance-field code, explicitly skip loaded meshes instead of treating them as planes.
- [ ] Run contracts.

### Task 6: GPU GBuffer arbitrary loaded meshes

**Files:**
- Modify: `Sources/Renderer/Gpu/GBufferGpu.hpp`
- Modify: `Sources/Renderer/Gpu/GBufferGpu.cpp`
- Modify: `Sources/Renderer/Gpu/DirectLightingGpu.hpp`

- [ ] Add a failing structural contract requiring loaded-mesh batches and requiring analytic primitive shadow/BVH lists to skip loaded meshes.
- [ ] Generalize mesh upload to accept `MeshData`/`MeshComponent` and maintain loaded batches keyed by loaded-mesh handle.
- [ ] Lazily allocate one persistent VAO/VBO/EBO/instance buffer per loaded handle when encountered; no per-frame geometry upload.
- [ ] Draw and destroy loaded batches alongside cube/plane batches.
- [ ] Skip loaded meshes from the current cube/plane-only analytic direct-light shadow primitive representation.
- [ ] Run source contracts and compile checks.

### Task 7: Generic build source discovery

**Files:**
- Modify: `build.c`
- Create/modify: `tests/build_sources_contract.cpp` or equivalent source contract.

- [ ] Write a failing contract rejecting `Sources/Ecs/*.cpp`, `Sources/Renderer/Lumen/*.cpp`, and other named subsystem patterns.
- [ ] Replace them with `Sources/*.cpp`, `Sources/*/*.cpp`, and `Sources/*/*/*.cpp`; add a deeper generic depth only if a current `.cpp` requires it.
- [ ] Verify all current production `.cpp` paths are covered exactly once.

### Task 8: Sponza default scene

**Files:**
- Modify: `Sources/Renderer/Test/TestScene.cpp`
- Modify: `Sources/Renderer/Test/TestScene.hpp` if scene state needs loaded entity storage.
- Create/modify: `tests/models_sponza_scene_contract.cpp`.

- [ ] Write a failing contract requiring the no-test scene to call `Models::loadInto(*world, "Assets/Sponza/sponza.obj", ...)` while named RendererCheck tests retain procedural cube/plane setup.
- [ ] Add a camera/light transform suitable for Sponza and do not animate Sponza submeshes as the old cube was animated.
- [ ] Keep all named test behavior unchanged.
- [ ] Run the scene contract.

### Task 9: Verification

- [ ] Initialize submodules and confirm `Assets/Sponza/sponza.obj` and `.mtl` exist.
- [ ] Run all new model contracts.
- [ ] Run existing contract suite.
- [ ] Run `c build` with strict warnings.
- [ ] If a GL-capable local environment is available, launch the normal scene and confirm Sponza geometry is submitted through loaded GPU batches.
- [ ] Inspect the final diff for parser work in hot paths, subsystem-specific `c_sources` lines, third-party model-loader includes, or accidental RendererCheck scene changes.
