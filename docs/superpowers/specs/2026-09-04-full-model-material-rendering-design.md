# Full Model Material Rendering Design

Date: 2026-09-04

## Goal

Make imported OBJ/MTL assets render with their actual material appearance instead of merely parsing and retaining metadata.

For imported models, material support is complete only when every currently supported MTL scalar and texture semantic either visibly affects rendering or is explicitly converted/preprocessed into another renderer input. Parsing a value without using it does not count as support.

The implementation remains fully self-contained: no Assimp, tinyobjloader, stb_image, or other third-party asset/image dependency.

The public model-facing API remains minimal and existing procedural ECS/RendererCheck scenes remain valid.

## Current gap

`Models::MaterialData` already stores a broad set of scalar properties and texture references, and `Models::spawn()` copies many scalar values into `Ecs::MaterialComponent`.

The interactive GPU path currently drops most of that information. `GBufferGpu` uploads only albedo, metallic, emissive and roughness. UVs exist in mesh vertices but are not forwarded to the fragment shader, and no imported texture is decoded, bound or sampled.

That is why the current Sponza render is essentially grayscale despite its MTL referencing many TGA textures.

Imported meshes are also currently rasterized but excluded from the analytic cube/plane GPU trace structure, so their triangles do not correctly participate in GPU shadow/Lumen/reflection rays.

This design closes both gaps.

## Non-goals

- No material editor GUI.
- No desktop asset-authoring tool.
- No third-party model/image library.
- No arbitrary image-format support in this phase; TGA is implemented thoroughly because it is the first imported asset format required by Sponza.
- No change to existing named RendererCheck fixture construction except dedicated new material tests.

## External API

Normal callers continue to use:

```cpp
Models::ModelHandle model = Models::load(path, &error);
Models::spawn(world, model, options, &error);
```

or:

```cpp
Models::loadInto(world, path, options, &error);
```

Callers never manually decode images, create GPU materials, bind textures, classify transparency or build ray acceleration structures.

## End-to-end flow

```text
OBJ geometry + mtllib/usemtl
        |
        v
MTL scalars + texture references/options
        |
        v
Models::MaterialData
        |
        +-- native TGA decode/cache
        +-- semantic resolution
        +-- tangent generation
        +-- genuine displacement preprocessing
        |
        v
renderer material resource
        |
        v
ECS material-resource handle
        |
        +------------------------------+
        |                              |
        v                              v
Opaque / Masked                 Transparent / Transmissive
        |                              |
        v                              v
GBuffer + textured material      forward transparent pass
        |                              ^
        v                              |
advanced direct PBR ------------------+
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

## Native TGA subsystem

Add:

- `Sources/Models/Tga.hpp`
- `Sources/Models/Tga.cpp`
- `Sources/Models/Texture.hpp`
- `Sources/Models/Texture.cpp`

The decoder supports:

- uncompressed true-color
- RLE true-color
- uncompressed grayscale
- RLE grayscale
- color-mapped TGA
- valid 8-bit grayscale/index data
- 15/16-bit color
- 24-bit color
- 32-bit color
- alpha channels
- top/bottom origin
- left/right origin
- BGR/BGRA conversion to internal RGBA8
- strict dimensions/type/depth validation
- strict truncated-data rejection
- strict RLE packet validation

Decoded images contain normalized source path, width, height, RGBA8 pixels and whether meaningful alpha exists.

CPU decode cache is keyed by normalized path. Equivalent paths decode once. CPU decode and GPU texture lifetime remain separate so parser/unit tests do not require GL.

## Texture color space

Color/sRGB roles:

- base color/diffuse
- ambient color
- specular color
- emissive
- reflection color
- transmission color

Linear/data roles:

- normal
- bump
- metallic
- roughness
- shininess
- opacity
- displacement
- clearcoat
- clearcoat roughness
- sheen data
- anisotropy

Raster upload uses appropriate sRGB or linear internal formats.

Ordinary color/data textures may use GPU mip generation. Normal maps use a CPU-generated mip chain whose vectors are renormalized after filtering.

## Texture options

All currently parsed `TextureRef` options become functional:

- `-o`: add `offset.xy` after scaling
- `-s`: multiply source UV by `scale.xy`
- `-t`: add `turbulence.xy` after scale/offset as the static MTL texture-space perturbation currently representable by this renderer
- `-clamp`: clamp transformed UV to `[0,1]`; otherwise repeat with `fract`
- `-bm`: multiply bump/displacement strength
- `-imfchan`: select scalar channel (`r`, `g`, `b`, `m`, `l`, `z` where supported by the parser); unsupported channel letters are a material diagnostic and use the semantic default channel

Resolved UV calculation is therefore:

```text
uv' = uv * scale.xy + offset.xy + turbulence.xy
```

then clamp/repeat behavior is applied.

## Material registry boundary

Introduce a renderer-facing material-resource layer, conceptually:

- `Sources/Renderer/Material/Material.hpp`
- `Sources/Renderer/Material/Material.cpp`

Dependency direction is fixed:

```text
Models parser -> Renderer material registration -> ECS handle -> renderer
```

The renderer never depends on OBJ/MTL parser internals and the ECS never stores GL object IDs.

`Ecs::MaterialComponent` remains valid for procedural scalar materials. Imported materials additionally carry a renderer material-resource handle.

A renderer material resource stores:

- resolved scalar/color values
- texture handles and texture options
- property-presence flags needed to distinguish an explicit zero from a default zero
- render class
- legacy-to-PBR conversions
- raw MTL values useful for diagnostics

## Render classes

Each resolved material is exactly one of:

- `Opaque`
- `Masked`
- `Transparent`
- `Transmissive`

`Opaque` and `Masked` use the deferred GBuffer path.

`Masked` discards fragments below alpha cutoff `0.5` after all scalar/texture opacity multiplication.

`Transparent` is fractional alpha without physical transmission and uses a forward pass after opaque Lumen composition.

`Transmissive` has explicit transmission/refraction semantics and uses the same forward stage with IOR/Fresnel/refraction.

## Exact opacity and Sponza compatibility rules

Raw `d` and `Tr` values remain preserved.

Ordinary MTL semantics are the default:

- explicit `d`: opacity = `clamp(d, 0, 1)`
- explicit `Tr`: opacity = `1 - clamp(Tr, 0, 1)`
- when both are explicitly present, the later directive in the MTL wins, matching parser order
- when neither is present, scalar opacity = `1`

The first Sponza MTL uses an exporter convention where many visually opaque materials contain `d 0`, while `map_d` materials contain `d 1`.

Detect compatibility mode only when all of the following are true at MTL-document scope:

1. there are at least 3 candidate textured materials without `map_d`, `Tr`, or explicit transmission;
2. at least 75% of those candidates explicitly contain `d <= 1/255`;
3. there is at least one `map_d` material and at least 75% of `map_d` materials with explicit `d` use `d >= 254/255`;
4. no explicit `Tr`/transmission pattern contradicts that convention.

Only under this detected mode, a non-`map_d`, non-transmissive material with explicit `d <= 1/255` resolves scalar opacity to `1` instead of `0`.

`map_d` remains authoritative for coverage and raw `d` is never rewritten in parsed metadata.

## Opacity-map classification

The selected scalar channel of the fully decoded opacity map is scanned.

A texel is considered binary-low when `value <= 1/255` and binary-high when `value >= 254/255`.

If every texel is binary-low or binary-high, the material is `Masked`.

If any texel lies strictly between those thresholds, the material is `Transparent` unless explicit transmission makes it `Transmissive`.

This gives Sponza foliage/chains a cutout path while preserving genuinely smooth opacity maps.

## Exact material value resolution

The parser/resolver must track whether each scalar/color property was explicitly present so an explicit zero is not confused with an absent value.

### Base color

```text
base = explicit Kd ? Kd : white
if map_Kd exists: base *= sample_srgb(map_Kd).rgb
```

### Ambient reflectance

```text
ambient = explicit Ka ? Ka : white
if map_Ka exists: ambient *= sample_srgb(map_Ka).rgb
```

`Ka` is ambient reflectance, not ambient occlusion.

### Emissive

```text
emissive = explicit Ke ? Ke : (map_Ke exists ? white : black)
if map_Ke exists: emissive *= sample_srgb(map_Ke).rgb
```

### Metallic

```text
metallic_base = explicit Pm ? Pm : (map_Pm exists ? 1 : 0)
metallic = metallic_base * selected_scalar(map_Pm, default=1)
```

clamped to `[0,1]`.

### Roughness

Priority is exact:

1. if `map_Pr` exists, `roughness = selected_scalar(map_Pr) * (explicit Pr ? Pr : 1)`;
2. else if explicit `Pr`, use `Pr`;
3. else if shininess texture exists, convert sampled shininess to roughness;
4. else if explicit `Ns`, convert `Ns` to roughness;
5. else use `1`.

Legacy conversion:

```text
roughness = sqrt(2 / (max(Ns,0) + 2))
```

then clamp to renderer interval `[0.04,1]`.

### Specular/F0

Start with dielectric F0 from IOR when metallic is not 1:

```text
ior = explicit Ni ? max(Ni, 1.0001) : 1.5
F0_ior = ((ior - 1) / (ior + 1))^2
```

If explicit `Ks` exists, it replaces the neutral dielectric F0 color before specular-strength modulation.

If `map_Ks` exists, sampled sRGB specular color multiplies `(explicit Ks ? Ks : white)` and becomes the explicit specular color.

`Ps`/specular-strength multiplies the dielectric/specular term when present; absent value defaults to `1`.

For metallic response, base color continues to drive conductor-like colored F0 and is blended with dielectric/specular F0 using metallic.

### Opacity

Resolved scalar opacity from `d`/`Tr`/compatibility is multiplied by `map_d` selected scalar when present.

No `map_d` means texture opacity multiplier `1`.

### Transmission

```text
transmission_amount = explicit Pt ? clamp(Pt,0,1) : 0
if transmission texture exists:
    transmission_amount *= selected_scalar(texture)
transmission_color = explicit Tf ? Tf : white
if transmission-color texture exists:
    transmission_color *= sample_srgb(texture).rgb
```

Any nonzero resolved transmission classifies the material as `Transmissive`.

### Clearcoat

```text
clearcoat = (explicit Pc ? Pc : (map_Pc exists ? 1 : 0)) * scalar(map_Pc, default=1)
clearcoat_roughness = (explicit Pcr ? Pcr : (map_Pcr exists ? 1 : 0.1)) * scalar(map_Pcr, default=1)
```

Both clamp to `[0,1]`; clearcoat roughness is clamped to `[0.04,1]` when clearcoat is active.

### Sheen

```text
sheen = (explicit sheen ? sheen : (sheen texture exists ? 1 : 0)) * scalar(sheen texture, default=1)
```

clamped to `[0,1]`.

### Anisotropy

```text
anisotropy = (explicit anisotropy ? anisotropy : (anisotropy texture exists ? 1 : 0)) * scalar(anisotropy texture, default=1)
```

clamped to `[-0.95,0.95]`.

### Reflectivity

```text
reflectivity = explicit reflectivity ? reflectivity : (reflection texture exists ? 1 : 0)
```

The reflection texture modulates reflection color/intensity; it does not replace base color.

## Texture slots

Every currently parsed texture slot must be sampled or intentionally preprocessed:

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

There is no supported slot that remains metadata-only after this work.

## Tangent generation

Extend renderer mesh vertices with tangent direction and handedness/sign.

For each triangle:

1. compute tangent/bitangent from position and UV gradients;
2. accumulate into shared vertices;
3. Gram-Schmidt orthogonalize tangent against final normal;
4. normalize;
5. derive handedness from the accumulated bitangent.

If the UV determinant magnitude is below `1e-8`, use a deterministic orthogonal fallback tangent derived from the normal. No NaNs are permitted.

## Normal and bump maps

True normal maps are sampled in linear color space, remapped from `[0,1]` to `[-1,1]`, normalized, then transformed through TBN to world space.

Bump maps use finite differences in texture space and `-bm` strength to perturb tangent-space normal.

When both exist, bump perturbation is applied first to the flat tangent-space basis and the decoded normal map is then composed/renormalized with it; neither is silently discarded.

## Sponza `_ddn` rule

A `map_Disp` path whose basename contains `_ddn` immediately before the extension is treated as a normal/detail-normal texture.

Example:

```text
textures/sponza_arch_ddn.tga -> normal/detail normal
```

All other `map_Disp` textures remain displacement maps.

The raw directive/path remains preserved for diagnostics.

## Genuine displacement

A genuine displacement texture is applied during static imported-mesh preprocessing:

1. transform UV using the texture options;
2. sample selected height channel;
3. convert sample from `[0,1]` to signed height `sample - 0.5`;
4. multiply by resolved `-bm`/displacement strength;
5. offset each source vertex along its current normal;
6. recompute affected normals;
7. regenerate tangents.

This is vertex displacement, not tessellation. It cannot create geometric detail below the source mesh's vertex density; that limitation is explicit.

## GPU raster batching

GL4.3 portability does not assume bindless textures.

Imported raster batches are keyed by:

```text
mesh handle + material handle + render class
```

A material's texture set is bound once per batch and all instances in that batch are drawn together.

Persistent VAO/VBO/EBO/instance-buffer behavior remains. Adding materials must not re-upload static mesh geometry every frame.

## GBuffer shader path

The vertex stage forwards:

- world position
- world normal
- transformed tangent + handedness basis
- UV
- batch/material data required by the fragment stage

The fragment stage resolves scalar + texture values and writes material-complete primary-surface data.

Masked materials discard below alpha `0.5` before writing depth/GBuffer.

## Expanded GBuffer semantics

The GL4.3 path uses up to the guaranteed eight draw buffers with this logical contract:

1. `PositionDepth`: world position + depth
2. `NormalRoughness`: resolved world normal + roughness
3. `AlbedoMetallic`: resolved base color + metallic
4. `EmissiveOpacity`: emissive + opacity
5. `SpecularIor`: resolved F0/specular RGB + IOR
6. `Advanced`: clearcoat + clearcoat roughness + sheen + reflectivity
7. `Transmission`: transmission tint RGB + amount
8. `TangentAnisotropy`: world tangent + anisotropy

Formats may be optimized for bandwidth/precision, but these semantics may not be removed.

A constant property may move to a material SSBO only when later passes can resolve it without losing per-pixel textured behavior.

## Direct PBR

Direct lighting becomes material-driven and includes:

- diffuse response
- GGX base specular
- IOR-derived dielectric F0
- explicit `Ks`/specular map/strength
- metallic response
- roughness
- clearcoat secondary GGX lobe
- sheen
- anisotropic GGX using tangent/bitangent
- emissive

Clearcoat energy is taken from the underlying base lobe using Fresnel weighting so clearcoat does not simply add unbounded energy.

Sheen is a grazing-angle fabric lobe and is energy-limited against diffuse.

Anisotropy modifies tangent/bitangent roughness axes rather than only scaling brightness.

## Ambient/indirect property

`Ka` and ambient texture modulate legacy ambient/indirect reflectance where used. They do not become AO and do not replace Lumen occlusion.

## Reflection property

Resolved reflectivity/reflection texture modulates the existing reflection result before final material composition.

Reflection data does not alter diffuse base color.

## Transparent/transmissive pass

Add a focused forward subsystem, conceptually:

- `Sources/Renderer/Gpu/TransparentGpu.hpp`
- `Sources/Renderer/Gpu/TransparentGpu.cpp`

It runs after opaque Lumen composition and receives:

- opaque final color
- opaque depth
- camera matrices/position
- direct-light/trace scene data
- transparent/transmissive material textures
- transparent mesh batches

It supports:

- fractional alpha
- transmission amount/color
- IOR
- Fresnel
- screen-space refraction of opaque color
- specular reflection
- direct lighting
- emissive
- tint/absorption approximation

Batches are sorted back-to-front by camera-space submesh/instance center each frame.

Opaque depth is read for rejection/occlusion. Transparent layers blend without destructively overwriting opaque depth.

This is sorted forward transparency, not full order-independent transparency; intersecting transparent geometry can therefore retain normal sorted-transparency limitations.

## Imported geometry trace scene

Imported meshes must participate in GPU shadows, GI occlusion, Lumen rays and off-screen reflection tracing.

Use a shared BLAS/TLAS structure:

```text
immutable loaded mesh
    -> local triangles
    -> per-mesh BLAS built once

ECS instances
    -> transforms/world bounds
    -> TLAS over instances

ray
    -> TLAS
    -> transform into mesh local space
    -> BLAS
    -> triangle hit
```

Procedural cube/plane analytic primitives remain as optimized primitive types and coexist with imported triangles.

Transform-only changes update/refit TLAS and never rebuild immutable mesh BLAS.

Trace triangles retain positions, required normals, UVs and material handle.

## Ray-visible textures under GL4.3

Raster material binding is insufficient for arbitrary off-screen ray hits because compute shaders cannot rely on portable bindless texture access.

Build internal trace texture atlases using fixed-size pages stored as `GL_TEXTURE_2D_ARRAY` layers.

Maintain at least two atlas classes:

- sRGB/color
- linear/data

Each material texture record stores:

- atlas class
- page/layer
- normalized region offset/scale
- clamp/repeat mode
- selected channel
- semantic flags

Each packed image receives a 4-texel gutter at mip 0, scaled appropriately through the mip chain. Gutters duplicate edge texels. Mip levels are generated per source region before packing so neighboring regions cannot bleed.

Repeat/clamp happens in source UV space before atlas remap.

If one page cannot fit the next texture, allocate another array layer; atlas growth never silently downsamples a source merely to make it fit. A texture exceeding the device maximum texture size is a recoverable material diagnostic with semantic fallback.

## Alpha-tested ray hits

Masked imported triangles use their opacity atlas during:

- direct-light shadow rays
- Lumen GI rays
- reflection rays

A hit whose resolved alpha is `< 0.5` is rejected and traversal continues.

Thus leaves/chains do not become solid rectangular occluders.

Fractional transparent/transmissive hits use an attenuation/transmission approximation rather than acting as fully opaque blockers.

## Material-aware Lumen

Primary visible surfaces enter Lumen through the expanded textured GBuffer.

Off-screen imported ray hits resolve from trace material/atlas data at least:

- base color
- normal/detail normal where the hit path has tangent/UV data
- metallic
- roughness
- emissive
- specular/F0
- reflectivity
- clearcoat
- opacity mask

This allows imported textured geometry to contribute to GI/occlusion/reflections instead of only receiving lighting.

Emissive texture × emissive scalar contributes to both visible emissive and imported-hit radiance used by Lumen.

## Resource ownership/lifetime

Explicit lifetime layers:

- CPU model geometry cache
- CPU decoded texture cache
- renderer material-resource cache
- GPU raster textures
- GPU trace atlases
- imported BLAS
- TLAS instance scene

`Models::clearCache()` must either release model-owned renderer resources in a defined safe order or delegate to one renderer/model-resource teardown owner. No ECS entity stores a live GL object ID.

## Change tracking

Track separately:

- transforms
- loaded mesh handle
- material-resource handle
- procedural material scalar values
- texture/material resource changes
- lights

A camera move never re-decodes/re-uploads textures or rebuilds BLAS.

A transform-only move updates/refits TLAS.

A material change invalidates affected material/raster/trace state without re-uploading immutable mesh buffers.

## Performance requirements

- Decode each normalized texture path once.
- Upload each raster texture once per required color-space interpretation.
- No per-frame filesystem reads.
- No per-frame TGA decode.
- No per-frame material registration.
- Build immutable mesh BLAS once.
- Refit/update TLAS for transform-only changes.
- Preserve persistent mesh/instance buffers.
- Batch raster draws by mesh + material + render class.
- Keep existing procedural RendererCheck perf scenarios procedural unless explicitly testing imported materials.

## Failure handling

Fatal:

- unreadable OBJ
- structurally invalid OBJ preventing geometry creation
- invalid required geometry indices

Recoverable material diagnostics:

- missing optional texture
- malformed optional TGA
- unsupported TGA variant outside this spec
- texture larger than device limits
- unsupported optional material directive outside currently documented parser support

Recoverable texture errors use deterministic semantic fallbacks instead of undefined sampling.

Fallbacks:

- base color: white
- ambient multiplier: white
- normal: `(0,0,1)` tangent-space
- bump: flat
- metallic: `0`
- roughness: resolved scalar or `1`
- specular: resolved IOR/scalar F0
- emissive: black
- opacity: `1`
- transmission: `0`
- clearcoat: `0`
- clearcoat roughness: `0.1` when needed
- sheen: `0`
- anisotropy: `0`
- reflection: resolved scalar/default

## Test strategy

All production behavior is implemented test-first.

### TGA contracts

- uncompressed 24-bit
- uncompressed 32-bit alpha
- RLE true-color
- grayscale
- color-mapped
- origin/orientation combinations
- malformed header
- truncated data
- malformed RLE packet
- cache reuse

Tiny byte-array/file fixtures are generated in tests; no large binary fixture is required.

### Material-resolution contracts

- every scalar property
- every texture slot
- property-presence handling
- sRGB vs linear classification
- `-o`, `-s`, `-t`, `-clamp`, `-bm`, `-imfchan`
- exact precedence rules above
- `Ns` -> roughness
- `Ni` -> F0
- ordinary `d`/`Tr`
- exact Sponza compatibility thresholds
- binary mask vs fractional alpha thresholds
- `_ddn` reclassification

### Geometry contracts

- tangent generation
- mirrored UV handedness
- degenerate UV fallback
- displacement
- normal/tangent recomputation after displacement

### GPU/source contracts

When no real GL context exists, strict source/packing contracts verify:

- material GPU layout
- raster batch key includes material/render class
- GBuffer shader forwards UV/tangent
- texture samplers are present
- masked alpha discard exists
- expanded GBuffer outputs exist
- direct-light shader consumes advanced channels
- transparent pass wiring
- imported trace-scene wiring

### Trace contracts

- BLAS build
- TLAS build/refit
- triangle hit
- transformed instance hit
- material handle propagation
- masked hit rejection and continued traversal
- procedural analytic + imported triangle coexistence

### Sponza contract

Using `Assets/Sponza`, verify:

- diffuse maps discovered
- `_ddn` resolves as normal/detail normal
- opacity masks discovered
- expected render classes
- exporter `d 0` opaque materials stay visible only under detected compatibility mode
- repeated texture paths deduplicate
- renderer material resources are created

Sponza does not replace procedural RendererCheck fixtures.

## Visual acceptance

Normal interactive Sponza must no longer appear as an untextured grayscale scene.

The frame must visibly demonstrate:

- colored diffuse textures
- brick/column/fabric normal detail
- correct leaf/chain cutouts
- material-dependent specular response
- roughness variation
- mapped normals
- imported geometry casting/receiving ray-based shadows/GI

Dedicated fixtures must separately demonstrate:

- metallic
- smooth vs rough specular
- clearcoat
- sheen
- anisotropy
- fractional transparency
- transmission/refraction with IOR
- genuine displacement
- emissive GI

## Compatibility

Existing procedural scenes using only scalar `Ecs::MaterialComponent` continue to work without imported material resources.

Existing model API calls remain valid.

Named RendererCheck scenes remain deterministic/procedural unless a dedicated new material test is intentionally added.

The normal no-test scene continues to use Sponza as the first imported model.

## Implementation stages

Every stage must leave `main` buildable and avoid unnecessary GitHub Actions runs.

1. native TGA decoder/cache
2. exact material resolution + Sponza compatibility
3. tangents + displacement preprocessing
4. renderer material registry + ECS resource handle
5. textured opaque/masked GBuffer
6. advanced direct PBR
7. transparent/transmissive forward pass
8. imported BLAS/TLAS trace scene
9. trace atlases + material-aware shadow/Lumen/reflection hits
10. Sponza visual/runtime verification + regression cleanup

Each stage gets a failing contract first, then implementation, then a focused verification gate before commit.

## Completion definition

This work is complete only when:

- every currently parsed material scalar has a defined renderer role;
- every currently parsed texture slot is sampled or intentionally preprocessed;
- native TGA decode requires no external library;
- Sponza renders textured and materially differentiated;
- opacity masks work;
- fractional transparency/transmission work;
- IOR/specular/metallic/roughness/clearcoat/sheen/anisotropy affect shading;
- genuine displacement affects static imported geometry;
- imported triangles participate in GPU shadows/Lumen/reflection tracing;
- procedural RendererCheck behavior remains intact;
- strict compile/tests pass for touched code;
- final full `c build` and interactive visual run are executed in a complete CrapGame/lwcgl environment.
