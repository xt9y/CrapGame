# Models subsystem design

## Goal

Add a self-contained `Sources/Models/` subsystem beside `Sources/Ecs/` and `Sources/Renderer/` that loads real model assets with a minimal public API, integrates loaded geometry/materials with the ECS and renderer, and does not depend on tinyobjloader, Assimp, or another model-loading library.

## Scope

The first native format is Wavefront OBJ with MTL materials. The implementation owns parsing, triangulation, material interpretation, model caching, mesh registration, and ECS spawning.

The loader must preserve materially useful data even when the current renderer does not sample every property yet. Missing optional data must degrade to stable defaults rather than making otherwise valid models unloadable.

## Public API

The public surface lives in `Sources/Models/Models.hpp` and should remain small:

```cpp
namespace Models
{
using ModelHandle = std::uint32_t;

struct SpawnOptions
{
    Ecs::TransformComponent transform;
    bool visible = true;
};

ModelHandle load(const std::string& path);
const Model& get(ModelHandle handle);
std::vector<Ecs::Entity> spawn(
    Ecs::World& world,
    ModelHandle model,
    const SpawnOptions& options = {}
);
std::vector<Ecs::Entity> loadInto(
    Ecs::World& world,
    const std::string& path,
    const SpawnOptions& options = {}
);
void clearCache();
}
```

`load()` parses/caches one model asset. `spawn()` creates one render entity per submesh/material assignment and gives all created entities the supplied transform. `loadInto()` is the convenience path. Internal parser/registry details stay inside `Sources/Models/`.

## Internal files

- `Sources/Models/Models.hpp`: minimal public API and public model/material data needed by callers.
- `Sources/Models/Models.cpp`: cache, public API implementation, ECS spawning, model handle validation.
- `Sources/Models/Obj.hpp`: internal OBJ/MTL parser interfaces and parser data structures.
- `Sources/Models/Obj.cpp`: OBJ parsing, MTL parsing, path resolution, triangulation, generated normals, submesh construction.
- `Sources/Models/Material.hpp`: texture-slot and extended material representation shared inside the subsystem and exposed only where ECS/render integration requires it.
- `Sources/Models/MeshRegistry.hpp/.cpp`: stable loaded-mesh handles and lookup of `Renderer::Mesh::MeshData` without storing renderer-owned vectors directly in ECS components.

The exact split may be reduced if two files would only contain trivial forwarding code, but the public API remains in `Models.hpp`.

## OBJ support

The parser supports:

- `v`, including optional vertex color values when present.
- `vt`.
- `vn`.
- `f` with `v`, `v/vt`, `v//vn`, and `v/vt/vn` tuples.
- Positive and negative OBJ indices.
- Triangles and arbitrary polygon faces, triangulated as a deterministic fan.
- `o` object names.
- `g` group names.
- `s` smoothing groups.
- `mtllib`, including multiple libraries.
- `usemtl` and material changes within an object/group.
- Comments, blank lines, CRLF/LF input, and leading/trailing whitespace.

Vertices are deduplicated by their resolved position/UV/normal tuple within each generated render submesh. If normals are missing, triangle normals are accumulated and normalized for smooth groups; smoothing-off faces receive face-normal behavior through vertex splitting.

Malformed required numeric/index data produces a descriptive load error containing file and line number. Unsupported OBJ statements are ignored rather than treated as fatal.

## Material model

A material can preserve these scalar/vector properties:

- base color / diffuse color
- ambient color
- specular color
- emissive color and emissive strength
- metallic
- roughness
- specular strength
- shininess
- index of refraction
- opacity
- transparency
- transmission / translucency
- reflectivity
- clearcoat and clearcoat roughness
- sheen
- anisotropy
- illumination model

The material also preserves texture references for:

- base color / diffuse
- ambient
- specular
- emissive
- metallic
- roughness
- shininess
- opacity / alpha
- normal
- bump
- displacement
- reflection
- transmission
- clearcoat
- clearcoat roughness
- sheen
- anisotropy

Texture references store the source path resolved relative to the MTL file and parsed map options that materially affect sampling, including offset, scale, clamp, bump multiplier, and channel selection when present.

Classic MTL properties are translated into PBR-oriented defaults where possible while retaining the original values. Common PBR MTL extensions such as `Pr`, `Pm`, and their map variants are supported.

## ECS integration

The existing procedural mesh path remains source-compatible.

`Ecs::MeshComponent` is extended so it can represent either:

1. an existing procedural `MeshType`, or
2. a loaded mesh handle owned by the Models mesh registry.

Existing `{MeshType::Cube}` and `{MeshType::Plane}` call sites must continue to compile unchanged.

`Ecs::MaterialComponent` retains the current fields and is extended with the additional scalar/vector surface properties needed by loaded models plus lightweight texture/material references. Existing aggregate initialization used by current scenes must remain valid through field ordering/default member initializers or call-site updates where required.

A loaded model with multiple submeshes/materials becomes multiple ECS entities. Those entities share the same transform value at spawn time and each receive `TransformComponent`, `MeshComponent`, `MaterialComponent`, and `RenderableComponent`.

## Renderer integration

`Renderer::Mesh` gains a single geometry-resolution path that accepts `Ecs::MeshComponent` and returns the correct `MeshData` for procedural or loaded meshes. Existing render backends must stop assuming every mesh is a `MeshType` enum.

The current rendering behavior for albedo, metallic, roughness, and emissive remains unchanged for procedural assets. Loaded scalar material properties feed those same paths immediately.

Texture paths and extended surface properties are preserved in ECS/model data in this stage. Texture image decoding/GPU upload and shader sampling are not hidden inside OBJ parsing. If texture sampling is added as part of the same implementation, it must live behind renderer-owned texture resources rather than OpenGL calls in `Sources/Models/`.

Transparency/transmission/reflectivity metadata must not be discarded merely because every rendering path does not yet consume it.

## Cache and lifetime

Model paths are normalized before cache lookup. Loading the same normalized path twice returns the same `ModelHandle` while the cache is alive.

Loaded model data and loaded mesh registry entries have stable handles for the lifetime of the cache. `clearCache()` invalidates model and loaded-mesh handles and is intended for teardown/tests, not per-frame use.

No model parsing, filesystem probing, material path resolution, or mesh allocation occurs in the frame hot path.

## Path handling

OBJ-relative MTL paths resolve relative to the OBJ file. Texture paths resolve relative to the MTL file that declares them. Absolute paths remain absolute. Path normalization must not require the referenced texture to exist, because preserving the material declaration is useful even when an optional texture is missing.

A missing OBJ is fatal for `load()`. A missing referenced MTL or texture is non-fatal: the model loads with defaults/preserved unresolved resource metadata and exposes a warning internally or through the load result/error channel selected during implementation.

## Build integration

`build.c` must stop enumerating individual subsystem directory names.

Because the current C-BuildSystem expands `c_sources()` with POSIX `glob()` rather than recursive globstar semantics, source registration uses generic depth patterns that cover every current source directory, including future `Sources/Models/` additions:

```c
c_sources(app, "Sources/*.cpp");
c_sources(app, "Sources/*/*.cpp");
c_sources(app, "Sources/*/*/*.cpp");
```

If repository inspection during implementation finds a deeper `.cpp` source level, add the corresponding generic `Sources/*/*/*/*.cpp` level. The final `build.c` must contain no per-subsystem source list such as `Sources/Ecs/*.cpp` or `Sources/Renderer/Lumen/*.cpp`.

## Testing

Tests must cover at least:

- triangle OBJ with positions only
- complete position/UV/normal tuples
- missing normals and generated normals
- negative indices
- quad/n-gon triangulation
- multiple objects/groups
- multiple materials and material changes
- multiple MTL libraries
- classic MTL scalar properties
- PBR extension properties
- every supported texture slot at parser-contract level
- relative OBJ -> MTL -> texture path resolution
- transparency/opacity semantics (`d` and `Tr`)
- malformed face/index data with useful errors
- missing MTL fallback
- model cache reuse
- loaded mesh registry lookup
- ECS `spawn()` entity/component creation
- procedural Cube/Plane compatibility
- renderer geometry resolution for procedural and loaded meshes

The main project must still build through `c build` with strict warnings enabled.

## Non-goals

- No Assimp, tinyobjloader, or similar dependency.
- No FBX implementation in this stage.
- No glTF implementation in this stage.
- No editor/GUI asset import workflow.
- No parsing or asset loading in the render loop.
- No renderer-specific OpenGL calls inside the OBJ/MTL parser.

## Success criteria

A caller can load a normal OBJ/MTL asset with one line, spawn it into `Ecs::World` with one additional call, and the renderer can consume its geometry and current PBR-compatible material values without caller knowledge of OBJ, MTL, submeshes, mesh registries, or parser internals. Existing procedural scene code remains functional and the build automatically includes `.cpp` files in current source-directory depths without naming each subsystem in `build.c`.
