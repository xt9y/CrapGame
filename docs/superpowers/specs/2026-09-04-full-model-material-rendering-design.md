# Full Model Material Rendering Design

Date: 2026-09-04

## Goal

Make imported OBJ/MTL assets render with their actual material appearance instead of merely parsing and retaining metadata.

For imported models, material support is considered complete only when supported MTL scalar values, texture maps, alpha/transparency behavior, normal/detail maps, displacement, advanced BRDF terms, emissive contribution, reflections, and imported-geometry ray visibility propagate from file parsing through ECS into the GPU renderer and visibly affect the final frame.

The implementation remains self-contained: no Assimp, tinyobjloader, stb_image, or other third-party asset/image dependency.

The current public model-facing API remains minimal. Existing procedural ECS scenes and RendererCheck fixtures remain valid.

## Current gap

The OBJ/MTL parser already stores a broad `Models::MaterialData`, including texture references and advanced scalar properties. `Models::spawn()` copies many scalar values into `Ecs::MaterialComponent`.

The interactive GPU path does not consume most of that data. `GBufferGpu` currently uploads only:

- albedo
- metallic
- emissive
- roughness

The mesh UV attribute exists, but the current GBuffer shader does not pass UVs to its fragment stage and does not bind or sample imported textures.

The consequence is the current Sponza result: geometry loads, but the scene is effectively shaded from uniform gray scalar values.

There is a second architectural gap: imported meshes are rasterized but deliberately excluded from the existing analytic cube/plane GPU acceleration structure, so imported triangles do not correctly participate in the GPU shadow/Lumen ray scene.

This design closes both gaps.

## Non-goals

- No generic editor/material GUI.
- No desktop asset authoring tool.
- No third-party image library.
- No new public model-loader complexity for ordinary callers.
- No change to named RendererCheck procedural fixture construction unless a dedicated new material test explicitly requires it.
- No requirement to support arbitrary image formats in this phase; TGA is the native image format required by the first imported asset and is implemented thoroughly.

## External model API

The existing model API remains the normal entry point:

```cpp
Models::ModelHandle model = Models::load(path, &error);
Models::spawn(world, model, options, &error);
```

or:

```cpp
Models::loadInto(world, path, options, &error);
```

Callers do not manually decode textures, create GPU materials, bind texture units, classify transparency, or build ray acceleration structures.

## End-to-end data flow

```text
OBJ
 |
 +-- geometry: positions / normals / UVs
 |
 +-- mtllib / usemtl
        |
        v
      MTL
        |
        +-- scalar properties
        +-- texture references/options
        |
        v
Models::MaterialData
        |
        +-- native TGA decode/cache
        +-- texture semantic classification
        +-- tangent generation
        +-- displacement preprocessing where applicable
        |
        v
renderer material resource
        |
        v
ECS material resource handle
        |
        +-----------------------------+
        |                             |
        v                             v
opaque / masked                 transparent / transmissive
        |                             |
        v                             v
GBuffer + material textures      forward transparent pass
        |                             ^
        v                             |
direct PBR ---------------------------+
        |
        v
Lumen / reflections
        |
        v
opaque composite
        |
        v
transparent composite
        |
        v
present
```

## Native TGA decoder

Add:

- `Sources/Models/Tga.hpp`
- `Sources/Models/Tga.cpp`
- `Sources/Models/Texture.hpp`
- `Sources/Models/Texture.cpp`

### Supported TGA forms

The decoder must support:

- uncompressed true-color
- RLE true-color
- uncompressed grayscale
- RLE grayscale
- color-mapped TGA
- 8-bit where valid for grayscale/index data
- 15-bit / 16-bit color
- 24-bit color
- 32-bit color
- alpha channels
- top/bottom origin
- left/right origin
- BGR/BGRA conversion to normalized internal RGBA8
- strict bounds checking
- truncated-file rejection
- malformed RLE rejection
- invalid dimensions/type/depth rejection

The decoded representation is a CPU image resource containing:

- normalized source path
- width
- height
- RGBA8 pixels
- whether meaningful alpha exists

### Texture cache

Texture files are cached by normalized filesystem path.

The same file referenced through equivalent relative paths decodes once.

CPU decode cache and GPU texture cache are separate lifetime layers so parser tests can run without an OpenGL context.

## Texture semantics and color space

Texture roles are classified at material resolution time.

sRGB/color textures:

- base color / diffuse
- ambient color
- specular color
- emissive
- reflection color
- transmission color

Linear/data textures:

- normal
- bump
- metallic
- roughness
- shininess
- opacity
- displacement/height
- clearcoat
- clearcoat roughness
- sheen scalar/data
- anisotropy

GPU upload must choose appropriate sRGB or linear internal formats rather than treating every texture as display color.

Mipmaps are generated for filtered color/data textures. Normal-map mip levels must be renormalized when generated on the CPU if GPU automatic mip generation would produce materially incorrect vectors.

## Texture-reference options

The existing `TextureRef` options must become functional:

- `-o`: UV offset
- `-s`: UV scale
- `-t`: stored texture-space turbulence/translation term
- `-clamp`: clamp vs repeat
- `-bm`: bump/displacement strength
- `-imfchan`: scalar channel selection

The resolved material resource stores these values per texture slot.

For repeat sampling, transformed UVs repeat before any atlas/page remap used internally by ray tracing.

For clamp sampling, transformed UVs clamp to the texture domain.

## Material registry boundary

The renderer must not depend directly on OBJ or MTL parser internals.

Introduce a renderer-facing material resource layer, conceptually under:

- `Sources/Renderer/Material/Material.hpp`
- `Sources/Renderer/Material/Material.cpp`

The exact file split may remain compact, but the dependency direction is fixed:

```text
Models parser -> Renderer material registration -> ECS handle -> renderer
```

A renderer material resource contains:

- resolved scalar values
- texture handles/slots
- texture transforms/options
- render class
- resolved legacy/PBR conversions
- raw values when useful for diagnostics

`Ecs::MaterialComponent` remains usable for procedural scalar materials. Imported materials additionally carry a renderer material-resource handle.

The ECS must not expose GPU object IDs.

## Render classes

Every resolved material is classified as one of:

- Opaque
- Masked
- Transparent
- Transmissive

### Opaque

No fractional coverage/transmission.

Uses deferred GBuffer path.

### Masked

Binary or effectively binary opacity coverage.

Uses deferred GBuffer path and discards fragments below an alpha threshold.

### Transparent

Fractional alpha/dissolve without physical transmission.

Uses a forward pass after the opaque Lumen composite.

### Transmissive

Material has explicit transmission/refraction semantics.

Uses the same forward stage with transmission, IOR and Fresnel handling.

## Sponza dissolve compatibility

The first Sponza MTL has an exporter convention that cannot be interpreted by applying ordinary `d` semantics blindly:

- many visually opaque materials contain `d 0.000000`
- alpha-masked materials contain `d 1.000000` plus `map_d`

Raw parsed `d`/`Tr` values remain preserved.

A separate resolved-opacity stage determines render behavior.

The compatibility rule must be narrow and data-driven, not a global inversion of MTL semantics.

At MTL-document scope, detect the legacy/exporter pattern when:

- a strong majority of textured, non-`map_d`, non-transmissive materials specify `d == 0`
- `map_d` materials use the opposite opaque scalar convention
- no competing `Tr`/transmission pattern establishes ordinary dissolve semantics

When this pattern is detected, `d == 0` on ordinary non-alpha/non-transmissive materials resolves to opaque while raw metadata remains unchanged.

Without this detected pattern, ordinary MTL dissolve semantics apply.

A `map_d` texture is still authoritative for coverage.

## Opacity-texture classification

After decoding an opacity texture:

- if sampled values are effectively binary, classify as Masked
- if intermediate values exist, classify as Transparent

This prevents foliage/chain cutouts from paying for a blended transparency path while still supporting genuinely smooth transparency maps.

The binary test uses a small epsilon around 0 and 1 and scans the decoded scalar channel selected by `-imfchan`.

## MTL property resolution

The material resolver must consume all currently parsed scalar properties:

- `Kd` base color
- `Ka` ambient reflectance
- `Ks` specular color
- `Ke` emissive
- `Tf` transmission color
- `Ns` shininess
- `Ni` index of refraction
- `d` dissolve
- `Tr` transparency
- `illum` illumination model
- `Pm` metallic
- `Pr` roughness
- `Ps` specular extension/strength
- `Pc` clearcoat
- `Pcr` clearcoat roughness
- `Pt` transmission
- reflectivity
- sheen
- anisotropy

Texture values modulate or replace their scalar counterparts according to the slot semantics below.

## Texture slots

Every currently parsed texture slot must be consumed or intentionally preprocessed:

- base color
- ambient
- specular
- emissive
- metallic
- roughness
- shininess
- opacity
- normal
- bump
- displacement
- reflection
- transmission
- clearcoat
- clearcoat roughness
- sheen
- anisotropy

Fallbacks are deterministic when a slot is absent.

## Value precedence

For metallic/roughness/specular-like properties:

```text
explicit texture sample
  -> explicit modern/PBR scalar
  -> legacy MTL conversion
  -> deterministic default
```

A texture generally modulates the resolved scalar unless the corresponding MTL extension defines it as the complete channel value. The exact choice is fixed per semantic and covered by tests.

## Legacy shininess to roughness

When explicit roughness is absent, convert `Ns` into microfacet roughness using a stable monotonic conversion such as:

```text
roughness = sqrt(2 / (Ns + 2))
```

with clamping to the renderer's supported roughness interval.

An explicit roughness scalar/map takes precedence.

## Specular and IOR

The direct-light path must no longer hardcode dielectric F0 to `0.04` when material data provides a better value.

For an ordinary dielectric without explicit specular color:

```text
F0 = ((ior - 1) / (ior + 1))^2
```

Explicit `Ks` / specular texture / specular-strength data then modifies the dielectric specular response according to material rules.

For metals, base color and metallic response continue to drive colored conductor-like F0.

## Tangent generation

Extend renderer mesh vertices with a tangent basis sufficient for tangent-space normal mapping and anisotropic shading.

Generated data includes:

- tangent direction
- handedness/sign used to reconstruct bitangent

Tangents are computed from triangle positions and UV gradients.

Degenerate UV triangles must not create NaNs. They receive a deterministic orthogonal fallback tangent derived from the normal.

Shared vertices accumulate tangents and normalize/orthogonalize against their final normal.

## Normal and bump maps

Normal maps are sampled in tangent space and transformed through TBN into world-space normals before writing the GBuffer.

Normal texture decoding uses linear color space.

Bump maps use local finite differences in texture space to perturb the tangent-space normal using `-bm` strength.

If both a true normal map and a bump map exist, both contribute in a defined order rather than silently discarding one.

## Sponza `_ddn` convention

The first Sponza asset frequently declares:

```text
map_Disp textures/*_ddn.tga
```

These files are normal/detail-normal textures, not height displacement maps.

Resolver rule:

- `map_Disp` path with `_ddn` normal-map naming convention -> normal/detail-normal slot
- otherwise `map_Disp` remains displacement

This rule is explicit, tested and isolated to semantic resolution; the raw directive remains preserved.

## Real displacement

A genuine displacement texture that is not reclassified as a normal map is applied during imported-mesh preprocessing.

For static OBJ geometry:

1. sample the height texture at vertex UV
2. apply selected channel and texture transform
3. multiply by `-bm`/resolved displacement scale
4. offset the vertex along its current normal
5. recompute affected face/vertex normals as necessary
6. regenerate tangents

This avoids introducing tessellation shaders solely for static OBJ displacement.

The limitation is explicit: vertex displacement cannot reproduce sub-vertex tessellation detail that does not exist in the source topology.

## GPU raster material path

OpenGL 4.3 portability does not assume bindless textures.

Imported raster work is grouped by a stable material-aware batch key:

```text
mesh handle + material handle + render class
```

For ordinary raster passes, the material texture set is bound once per batch, then all instances of that batch are drawn.

Persistent VAO/VBO/EBO and instance-buffer behavior remains; texture support must not re-upload mesh data every frame.

## GBuffer shader

The vertex stage must forward:

- world position
- world normal
- transformed tangent basis
- UV
- material/instance data needed by the batch

The fragment stage resolves the material by combining scalar data and bound textures.

At minimum it must visibly resolve:

- textured base color
- mapped normal/bump
- metallic
- roughness/shininess
- emissive
- alpha mask
- specular/F0
- IOR
- transmission data
- reflectivity
- clearcoat
- clearcoat roughness
- sheen
- anisotropy

## Expanded GBuffer layout

The target GL4.3 path may use the guaranteed minimum of eight draw buffers.

The preferred logical layout is:

1. `PositionDepth`: world position + depth
2. `NormalRoughness`: resolved world normal + roughness
3. `AlbedoMetallic`: resolved base color + metallic
4. `EmissiveOpacity`: resolved emissive + opacity
5. `SpecularIor`: resolved F0/specular color + IOR
6. `Advanced`: clearcoat + clearcoat roughness + sheen + reflectivity
7. `Transmission`: transmission tint RGB + transmission amount
8. `TangentAnisotropy`: world tangent + anisotropy

Exact internal formats are chosen for precision/bandwidth balance, but semantics remain stable.

If a property is constant and can safely remain in a material SSBO without requiring UV reconstruction in later passes, it may be sourced by material ID instead of consuming redundant GBuffer channels. That optimization must not remove any per-pixel textured behavior.

## Direct PBR lighting

The direct-light BRDF evolves from basic metallic/roughness GGX to material-driven evaluation.

Required contributions:

- Lambertian/diffuse response
- GGX specular
- IOR-derived dielectric F0
- explicit specular color/strength
- metallic response
- roughness
- clearcoat secondary GGX lobe
- sheen contribution
- anisotropic specular distribution using tangent/bitangent
- emissive

Energy sharing between diffuse/base specular/clearcoat must avoid obvious double counting.

## Ambient property

Legacy `Ka` / ambient texture is preserved as an ambient-reflectance term.

It must not be mislabeled as ambient occlusion.

In the GPU renderer it modulates the material's indirect/ambient response where a legacy ambient term is needed; it does not replace Lumen occlusion.

## Reflection property

Reflectivity and reflection texture modulate the reflection contribution reaching the final material response.

They do not merely tint base color.

The existing Lumen/reflection output remains the source of scene reflection; material reflectivity controls how strongly it is applied.

## Transmission and real transparency

Transparent and transmissive surfaces bypass the opaque GBuffer write and render in a separate forward stage after opaque Lumen composition.

Add a focused GPU subsystem, conceptually:

- `Sources/Renderer/Gpu/TransparentGpu.hpp`
- `Sources/Renderer/Gpu/TransparentGpu.cpp`

The pass receives:

- opaque final color
- opaque depth
- camera matrices/position
- direct-light data
- material resources/textures
- transparent mesh batches

It supports:

- fractional opacity
- transmission amount
- transmission color / `Tf`
- IOR
- Fresnel
- screen-space refraction of opaque scene color
- specular reflection contribution
- direct lighting
- emissive
- tint/absorption approximation

Transparent batches are depth-sorted back-to-front at submesh/instance granularity.

Opaque depth is read for occlusion but transparent surfaces do not overwrite opaque depth in a way that destroys later transparent layers.

The implementation is explicit about the limitation that simple sorted forward transparency is not full order-independent transparency.

## Imported geometry acceleration

Imported meshes must participate in GPU shadows, GI occlusion, Lumen rays and off-screen reflection tracing.

Introduce a shared imported-geometry trace scene rather than flattening every imported triangle into world space every frame.

Conceptual structure:

```text
static loaded mesh
    -> local triangles
    -> per-mesh BLAS BVH built once

ECS instances
    -> world transform + world bounds
    -> TLAS over instances

ray
    -> TLAS
    -> instance transform to local space
    -> BLAS
    -> triangle intersection
```

Procedural cube/plane analytic primitives remain as an optimized trace primitive type and coexist with imported BLAS/TLAS geometry.

The shared trace scene is consumed by both direct-light shadow queries and Lumen trace queries.

## Triangle trace records

Imported trace triangles retain enough data to recover material response at a hit:

- positions
- geometric/smoothed normals as required
- UVs
- material handle

This enables alpha-tested ray hits and material-aware off-screen GI/reflection sampling.

## Ray-visible texture access under GL4.3

Compute-ray hits cannot rely on arbitrary bindless sampler access.

Raster batching continues to use ordinary per-material binding.

For ray-visible material textures, create an internal trace texture atlas representation.

The atlas uses one or more fixed-size pages stored as `GL_TEXTURE_2D_ARRAY` layers. Texture regions are packed with gutters. Material trace metadata stores:

- atlas class
- page/layer
- normalized region offset/scale
- clamp/repeat behavior
- channel selection

At minimum maintain separate atlas classes for:

- sRGB/color data
- linear/data maps

Mip levels are generated per source region before/while packing so neighboring atlas regions do not bleed into each other.

Repeat is performed in material UV space before remapping into the atlas region.

This permits arbitrary material counts in Lumen/shadow compute shaders using a small fixed sampler set.

## Alpha-tested ray hits

Masked geometry must respect opacity maps in:

- shadow rays
- Lumen GI rays
- reflection rays

A triangle intersection that lands on a masked-out texel is rejected and traversal continues.

This prevents leaf/chain billboard polygons from behaving as solid occluders.

Fractionally transparent/transmissive surfaces are not treated as fully opaque shadow blockers; their initial implementation uses a defined attenuation/transmission approximation in the forward/trace material path.

## Material-aware Lumen

Primary visible surfaces already enter Lumen through GBuffer data; after this upgrade that data is textured and materially complete.

Off-screen ray hits must resolve at least:

- base color
- normal/detail normal where practical for hit shading
- metallic
- roughness
- emissive
- specular/F0
- reflectivity
- clearcoat parameters
- opacity mask

This allows imported geometry to contribute textured GI/occlusion/reflection information instead of only receiving lighting.

The trace material path can intentionally omit expensive features that have negligible effect on bounce radiance only if the omission is explicit, tested and visually bounded; it cannot silently discard an entire supported material class.

## Emissive GI

Resolved emissive texture × emissive scalar/strength contributes both to visible GBuffer emissive and to imported-material radiance used by Lumen surface/ray evaluation.

An emissive imported surface must therefore be able to influence indirect lighting rather than only appear bright itself.

## Resource lifetime

Model cache ownership remains explicit:

- CPU model geometry cache
- CPU decoded texture cache
- renderer material resource cache
- GPU raster textures
- GPU trace atlases
- imported BLAS data

`Models::clearCache()` must invalidate model-owned renderer material/texture/mesh resources safely, or the API must be adjusted so cache teardown happens through one ordered ownership path.

No ECS entity may retain a live GPU object ID directly.

## Change tracking

Scene-change tracking must distinguish:

- transform changes
- loaded mesh/material handle changes
- material scalar changes
- texture/material-resource changes
- light changes

A pure camera move must not force texture re-upload or BLAS rebuild.

A transform move may refit/update TLAS without rebuilding static mesh BLAS.

A material texture change invalidates affected raster/material state and ray material state but not immutable mesh vertex buffers.

## Performance requirements

- Decode each source texture once.
- Upload each GPU raster texture once per required color-space interpretation.
- Build imported mesh BLAS once for immutable geometry.
- Refit/update TLAS for transform-only changes where possible.
- Preserve persistent mesh and instance buffers.
- Batch raster draws by mesh + material + render class.
- Do not perform per-frame filesystem reads.
- Do not perform per-frame TGA decode.
- Do not re-register materials every frame.
- Keep RendererCheck performance scenarios procedural unless explicitly testing imported materials.

## Failure handling

Fatal model-load errors:

- unreadable OBJ
- structurally invalid OBJ preventing geometry creation
- invalid required geometry indices

Material/texture failures are normally recoverable:

- missing optional texture -> diagnostic + semantic fallback
- malformed optional TGA -> diagnostic + semantic fallback
- unsupported optional material directive -> retain raw/ignore only when outside documented support

A missing opacity map on a material that explicitly depends on that mask falls back to a deterministic opaque/transparent policy and reports the missing asset rather than producing undefined sampling.

Fallback textures:

- base color: white
- ambient: white/neutral multiplier
- normal: `(0, 0, 1)` tangent-space
- bump: flat
- metallic: 0
- roughness: resolved scalar/default
- specular: resolved scalar/F0
- emissive: black
- opacity: 1
- transmission: 0
- clearcoat: 0
- clearcoat roughness: scalar/default
- sheen: 0
- anisotropy: 0
- reflection: neutral material reflectivity

## Testing strategy

All new behavior is implemented test-first.

### TGA contracts

- uncompressed 24-bit
- uncompressed 32-bit with alpha
- RLE true-color
- grayscale
- color-mapped image
- each origin/orientation combination
- malformed header
- truncated pixel data
- malformed RLE packet
- cache reuse

Fixtures are generated as tiny byte arrays/files in tests; no large binary test asset is required.

### Material resolution contracts

- every scalar property
- every texture slot
- sRGB vs linear classification
- `-o`
- `-s`
- `-t`
- `-clamp`
- `-bm`
- `-imfchan`
- value precedence
- `Ns` to roughness conversion
- `Ni` to F0 conversion
- `d` / `Tr` semantics
- Sponza dissolve-compatibility detection
- binary alpha mask vs fractional transparency
- `_ddn` reclassification

### Geometry contracts

- tangent generation
- mirrored UV handedness
- degenerate UV fallback
- displacement preprocessing
- normal/tangent recomputation after displacement

### GPU/source contracts

Where a real OpenGL context is unavailable in unit tests, source/packing contracts verify:

- material GPU record layout
- raster batch key includes material/render class
- texture samplers/UV path exist in GBuffer shader
- alpha discard exists for masked materials
- expanded GBuffer semantic outputs
- direct-light shader consumes advanced material channels
- transparent pass wiring
- imported trace-scene wiring

Runtime GL tests are added where the existing environment can execute them without destabilizing deterministic RendererCheck.

### Trace-scene contracts

- BLAS construction
- TLAS construction/refit
- triangle hit
- transformed instance hit
- masked hit rejection
- material handle propagation
- procedural analytic primitive coexistence

### Sponza asset contract

Using the existing `Assets/Sponza` submodule, verify at minimum:

- diffuse/base-color maps are discovered
- `_ddn` files resolve as normal/detail maps
- opacity masks are discovered
- material classes are sensible
- opaque exporter `d 0` materials remain visible under detected compatibility mode
- texture cache deduplicates repeated references
- renderer material resources are created for imported parts

The Sponza contract does not replace procedural RendererCheck fixtures.

## Visual acceptance criteria

Normal interactive Sponza rendering must no longer appear as an untextured grayscale model.

The resulting frame must visibly demonstrate:

- colored diffuse textures
- mapped brick/column/fabric detail
- correct leaf/chain cutouts
- material-dependent specular response
- roughness variation
- correct mapped normals
- emissive contribution if present in an imported material
- imported geometry casting/receiving correct ray-based shadow/GI effects

Separate dedicated fixtures must demonstrate:

- metallic material
- smooth vs rough specular
- clearcoat
- sheen
- anisotropy
- fractional transparency
- transmission/refraction with IOR
- real displacement

A feature is not considered implemented merely because the parser stores its value.

## Compatibility

Existing procedural scenes using only `Ecs::MaterialComponent` scalar values continue to render without requiring imported material resources.

Existing model API calls remain valid.

Named RendererCheck scenes remain deterministic and procedural unless a new named material test is intentionally added.

The normal no-test scene continues to use Sponza as the first imported model.

## Implementation staging constraint

Implementation should be staged so every stage leaves `main` buildable and does not trigger unnecessary GitHub Actions runs.

Recommended order:

1. native TGA decoder/cache
2. material resolution + Sponza compatibility
3. tangent/displacement preprocessing
4. renderer material registry + ECS handle
5. textured opaque/masked GBuffer
6. advanced direct PBR material response
7. transparent/transmissive forward pass
8. imported BLAS/TLAS trace scene
9. material-aware shadow/Lumen ray hits + trace texture atlases
10. final Sponza visual/runtime verification and regression cleanup

Each stage gets focused tests before production changes and should be committed only when its local gate is green.

## Completion definition

This work is complete only when:

- every currently parsed material scalar has a documented visible renderer role or a documented physically irrelevant fallback role
- every currently parsed texture slot is sampled, preprocessed, or intentionally converted into another supported representation
- TGA textures are decoded without external libraries
- Sponza renders textured and materially differentiated
- opacity masks work
- genuine transparency/transmission work
- advanced specular/IOR/clearcoat/sheen/anisotropy are wired into shading
- real displacement is processed for static imported meshes
- imported triangles participate in GPU shadows and Lumen/reflection tracing
- procedural RendererCheck behavior remains intact
- strict compile/tests pass for all touched code
- final full `c build` and interactive visual run are executed in an environment containing the complete lwcgl/CrapGame runtime
