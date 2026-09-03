# Models subsystem design

## Goal

Add a self-contained `Sources/Models/` subsystem beside `Sources/Ecs/` and `Sources/Renderer/` that loads real model assets with a minimal public API, integrates loaded geometry/materials with the ECS and renderer, and does not depend on tinyobjloader, Assimp, or another model-loading library.

## Scope

The first native format is Wavefront OBJ with MTL materials. The implementation owns parsing, triangulation, material interpretation, model caching, mesh registration, and ECS spawning.

The loader preserves materially useful data even when the current renderer does not sample every property yet. Missing optional data degrades to stable defaults rather than making otherwise valid models unloadable.

## Public API

The public surface lives in `Sources/Models/Models.hpp` and remains small:

```cpp
namespace Models
{
using ModelHandle = std::uint32_t;
constexpr ModelHandle INVALID_MODEL = UINT32_MAX;

struct SpawnOptions
{
    Ecs::TransformComponent transform = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f}
    };
    bool visible = true;
};

ModelHandle load(const std::string& path);
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

`load()` parses/caches one model asset. `spawn()` creates one render entity per submesh/material assignment and gives all created entities the supplied transform. `loadInto()` is the convenience path. Internal parser, model-data, material-data, and registry details stay inside `Sources/Models/`.

Fatal load/parser failures throw `std::runtime_error` with the source path and, for syntax failures, the line number. Passing an invalid or stale handle to `spawn()` also throws `std::runtime_error`.

## Internal files

The subsystem uses this fixed initial split:

- `Sources/Models/Models.hpp`: minimal public API only.
- `Sources/Models/Models.cpp`: cache, public API implementation, ECS spawning, model handle validation.
- `Sources/Models/Obj.hpp`: internal OBJ/MTL parser interfaces and parser data structures.
- `Sources/Models/Obj.cpp`: OBJ parsing, MTL parsing, path resolution, triangulation, generated normals, submesh construction.
- `Sources/Models/Material.hpp`: internal extended material and texture-slot representation.
- `Sources/Models/MeshRegistry.hpp`: loaded-mesh handle type and registry interface.
- `Sources/Models/MeshRegistry.cpp`: stable loaded-mesh storage and `Renderer::Mesh::MeshData` lookup.

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

Malformed required numeric/index data produces a `std::runtime_error` containing file and line number. Unsupported OBJ statements are ignored rather than treated as fatal.

## Material model

A material preserves these scalar/vector properties:

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

For classic opacity semantics, `d` is opacity and `Tr` is transparency, so effective opacity is `1 - Tr` when `Tr` is the active declaration. Values are clamped to `[0, 1]`.

## ECS integration

The existing procedural mesh path remains source-compatible.

`Ecs::MeshComponent` is extended so it can represent either:

1. an existing procedural `MeshType`, or
2. a loaded mesh handle owned by the Models mesh registry.

Existing `{MeshType::Cube}` and `{MeshType::Plane}` call sites continue to compile unchanged.

`Ecs::MaterialComponent` retains the current fields and is extended with the additional scalar/vector surface properties needed by loaded models plus lightweight texture/material references. Current scene initialization is kept compiling by preserving current fields and supplying defaults for newly added fields.

A loaded model with multiple submeshes/materials becomes multiple ECS entities. Those entities receive identical transform values at spawn time and each receive `TransformComponent`, `MeshComponent`, `MaterialComponent`, and `RenderableComponent`.

## Renderer integration

`Renderer::Mesh` gains one geometry-resolution path that accepts `Ecs::MeshComponent` and returns the correct `MeshData` for procedural or loaded meshes. Existing render backends stop assuming every mesh is only a `MeshType` enum.

The current rendering behavior for albedo, metallic, roughness, and emissive remains unchanged for procedural assets. Loaded scalar material properties feed those same paths immediately.

Texture paths and extended surface properties are preserved in ECS/model data in this stage. Texture image decoding, GPU upload, shader sampling, transparency blending, refraction, and transmission rendering are outside this stage. They are not faked in the OBJ parser and the parsed metadata is retained so renderer-owned texture/material systems can consume it later.

## Cache and lifetime

Model paths are normalized before cache lookup. Loading the same normalized path twice returns the same `ModelHandle` while the cache is alive.

Loaded model data and loaded mesh registry entries have stable handles for the lifetime of the cache. `clearCache()` invalidates model and loaded-mesh handles and is only valid when no live ECS world still references loaded meshes. It is intended for teardown/tests, not per-frame use.

No model parsing, filesystem probing, material path resolution, or mesh allocation occurs in the frame hot path.

## Path handling

OBJ-relative MTL paths resolve relative to the OBJ file. Texture paths resolve relative to the MTL file that declares them. Absolute paths remain absolute. Path normalization does not require the referenced texture to exist because preserving the material declaration is useful even when an optional texture is missing.

A missing OBJ is fatal for `load()` and throws. A missing referenced MTL is non-fatal: affected faces use the default material. A missing referenced texture is non-fatal: its resolved texture path and slot metadata remain stored. There is no public warning API in this stage.

## Build integration

`build.c` stops enumerating individual subsystem directory names.

The current C-BuildSystem expands `c_sources()` with POSIX `glob()` rather than recursive globstar semantics, so source registration uses generic depth patterns that cover the current source tree and future peer folders such as `Sources/Models/`:

```c
c_sources(app, "Sources/*.cpp");
c_sources(app, "Sources/*/*.cpp");
c_sources(app, "Sources/*/*/*.cpp");
```

The final `build.c` contains no per-subsystem source list such as `Sources/Ecs/*.cpp` or `Sources/Renderer/Lumen/*.cpp`. If a `.cpp` source exists deeper than these three source levels at implementation time, one corresponding generic depth pattern is added for that depth.

## Testing

Tests cover at least:

- triangle OBJ with positions only
- complete position/UV/normal tuples
- missing normals and generated normals
- negative indices
- quad/n-gon triangulation
- multiple objects/groups
- smoothing-group behavior
- multiple materials and material changes
- multiple MTL libraries
- classic MTL scalar properties
- PBR extension properties
- every supported texture slot at parser-contract level
- relative OBJ -> MTL -> texture path resolution
- transparency/opacity semantics (`d` and `Tr`)
- malformed face/index data with useful errors
- missing MTL fallback
- missing texture-path preservation
- model cache reuse
- loaded mesh registry lookup
- invalid/stale model handle rejection
- ECS `spawn()` entity/component creation
- procedural Cube/Plane compatibility
- renderer geometry resolution for procedural and loaded meshes

The main project still builds through `c build` with strict warnings enabled.

## Non-goals

- No Assimp, tinyobjloader, or similar model-loading dependency.
- No FBX implementation in this stage.
- No glTF implementation in this stage.
- No editor/GUI asset import workflow.
- No texture image decoding or GPU texture upload in this stage.
- No transparency/refraction/transmission render pass in this stage.
- No parsing or asset loading in the render loop.
- No renderer-specific OpenGL calls inside the OBJ/MTL parser.

## Success criteria

A caller can load a normal OBJ/MTL asset with one line, spawn it into `Ecs::World` with one additional call, and the renderer consumes its geometry plus the PBR-compatible scalar material values it already supports without caller knowledge of OBJ, MTL, submeshes, mesh registries, or parser internals. All supported texture and extended-material metadata survives loading for later renderer use. Existing procedural scene code remains functional and `build.c` automatically includes `.cpp` files by generic source-tree depth rather than naming each subsystem.
