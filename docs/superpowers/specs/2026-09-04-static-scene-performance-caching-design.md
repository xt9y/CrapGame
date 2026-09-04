# Static-Scene Performance and Caching Design

Date: 2026-09-04
Repository: `xt9y/CrapGame`
Target branch: `main`

## Goal

Make the interactive Sponza scene fast because its geometry, materials, and directional light are static, not by lowering whole-frame resolution or requiring faster hardware.

The renderer must avoid recomputing expensive work whose semantic inputs have not changed. Native primary raster resolution remains the default. Expensive secondary effects become cached, reprojected, tiled, or sparse.

All implementation commits for this work use the prefix `Perf: `.

## Constraints

- Keep the primary GBuffer/raster image at native window resolution.
- Do not introduce third-party rendering dependencies.
- Preserve existing RendererCheck deterministic reference behavior.
- Preserve correctness for dynamic lights, moving geometry, transparent/transmissive materials, masked alpha geometry, and camera motion.
- Prefer focused classes/files over expanding `Render.cpp` or monolithic GPU files.
- Cache validity is driven by explicit revisions and inputs, never by elapsed wall-clock time.
- A cache hit must never silently reuse data after one of its semantic inputs changed.
- No CPU readback or GPU synchronization is added to the normal gameplay hot path for cache decisions.

## Current Problem

The renderer already skips some work when ECS revisions are unchanged, but the expensive GPU path still repeats work that does not need to repeat.

In the normal Sponza scene:

- geometry is static;
- materials are static;
- one directional light is static;
- only the camera normally changes.

The current pipeline still makes camera motion trigger a full GBuffer redraw and direct-light recomputation, and Lumen can continue tracing merely because its time-based schedule is due. At very low frame rates, every rendered frame is automatically later than the Lumen interval, so time-based convergence can accidentally become continuous expensive tracing.

## Architecture Overview

```text
load scene
  |
  +-- preload meshes/textures/materials
  +-- build static triangle acceleration
  +-- precompile all shaders
  +-- build static directional shadow cache
  +-- initialize world-space radiance cache
  v
interactive frame
  |
  +-- static scene/light unchanged?
  |       yes -> retain static caches
  |       no  -> invalidate only affected caches
  |
  +-- camera unchanged?
  |       yes -> present cached converged final texture when valid
  |       no  -> raster native-resolution GBuffer
  |                |
  |                +-- reproject previous lighting/history
  |                +-- validate reusable pixels/tiles
  |                +-- shade only dirty/missing work
  |                +-- sample static directional shadows
  |                +-- sample world-space radiance cache
  |                +-- trace only sparse cache misses
  |
  +-- cheap view-dependent specular/reflection update
  +-- transparent/transmissive forward pass when needed
  +-- present
```

## 1. Explicit Revision Domains

Add renderer-side revision/state tracking that separates semantic domains instead of treating one world revision as one invalidation source.

Required domains:

- geometry revision
- material revision
- lighting revision
- camera revision
- resolution revision
- imported-mesh registry revision
- renderer-material registry revision

Each cache declares exactly which revisions invalidate it.

### Cache invalidation matrix

| Cache | Geometry | Material | Light | Camera | Resize |
| --- | --- | --- | --- | --- | --- |
| mesh/texture residency | registry only | registry only | no | no | no |
| static BLAS | yes | no | no | no | no |
| TLAS | transforms/visibility | no | no | no | no |
| static directional shadow | yes | alpha/masked | yes | no | no |
| world radiance cache | yes | yes | yes | no | no |
| reprojection history | yes | yes | yes | yes | yes |
| converged final frame | yes | yes | yes | yes | yes |
| static diffuse lighting cache | yes | yes | yes | reprojection only | yes |
| view specular cache | yes | yes | yes | yes | yes |

No cache keys directly on wall-clock time.

## 2. Converged Frame Cache

Create:

- `Sources/Renderer/Gpu/ConvergedFrameCache.hpp`
- `Sources/Renderer/Gpu/ConvergedFrameCache.cpp`
- `Sources/Renderer/Gpu/ConvergencePolicy.hpp`

Purpose: stop rendering expensive passes when the exact scene, light, camera, and resolution are unchanged and Lumen history has converged.

State:

- cached final texture handle/reference
- revision tuple
- convergence sample count
- convergence stable-count
- valid/converged flags

No full-frame GPU-to-CPU variance readback is allowed. Convergence is determined by revision stability plus bounded sample count:

1. Any relevant revision change resets sample/stable counts.
2. The first four samples after invalidation always run.
3. Samples 5-8 run only when the existing Lumen history-valid path requests refinement.
4. After eight valid samples with no semantic invalidation, the frame is considered converged.
5. Sixteen samples is the absolute maximum even under a debug/fixed refinement mode.
6. Once converged and revisions remain identical, skip geometry, direct, Lumen, and composite work and present the cached final texture.

Policy constants:

```text
MIN_CONVERGENCE_SAMPLES = 4
DEFAULT_CONVERGENCE_SAMPLES = 8
MAX_CONVERGENCE_SAMPLES = 16
```

A debug environment override may request a count from 1-16, but the default path uses 8.

## 3. Scene Prewarm

Create:

- `Sources/Renderer/Gpu/ScenePrewarm.hpp`
- `Sources/Renderer/Gpu/ScenePrewarm.cpp`

Purpose: move one-time residency/compilation work out of the first visible frame.

Prewarm after scene construction and renderer initialization, before the interactive frame loop.

Prewarm performs:

- upload every loaded mesh referenced by visible world entities;
- upload every referenced material texture;
- build mip chains;
- build imported BLAS/TLAS resources;
- rebuild trace-material GPU records/atlases;
- compile the imported direct-light shader;
- compile the imported Lumen shader;
- allocate GBuffer/direct/Lumen/transparent targets;
- build static directional shadow resources when applicable;
- allocate/initialize world-space radiance-cache resources.

The prewarm API returns explicit success/error. Failure must not silently fall back to per-frame lazy initialization.

Prewarm may make startup/loading slower; that cost is intentional because it removes multi-second visible first frames.

## 4. Static Directional Shadow Cache

Create:

- `Sources/Renderer/Gpu/StaticShadowCacheGpu.hpp`
- `Sources/Renderer/Gpu/StaticShadowCacheGpu.cpp`
- `Sources/Renderer/Gpu/StaticShadowShader.hpp`

Purpose: eliminate per-screen-pixel imported-triangle shadow traversal for a static directional light.

Use one 2048x2048 light-space depth atlas per active static directional light. The orthographic projection is fitted to the static world bounds with 5% padding on each axis.

Requirements:

- opaque geometry writes depth directly;
- masked geometry performs the same alpha cutoff semantics as the GBuffer before writing depth;
- transparent/transmissive geometry does not write opaque shadow depth;
- lookup uses a fixed 3x3 PCF kernel;
- cache keyed by geometry/material/light revisions;
- camera movement never invalidates this cache;
- dynamic lights and moving geometry retain the existing ray/BVH path;
- if a directional light or its transform changes, regenerate before the next frame that uses it.

The normal Sponza directional light must use the cached path after initial generation.

## 5. Reprojection Cache

Create:

- `Sources/Renderer/Gpu/ReprojectionCacheGpu.hpp`
- `Sources/Renderer/Gpu/ReprojectionCacheGpu.cpp`
- `Sources/Renderer/Gpu/ReprojectionShader.hpp`
- `Sources/Renderer/Gpu/ReprojectionPolicy.hpp`

Purpose: reuse previous expensive lighting when only the camera moves.

History stores:

- previous world position/depth
- previous normal
- previous exact 32-bit material ID
- previous cached static diffuse/direct contribution
- previous indirect lighting
- previous reflection contribution

For each current pixel:

1. Reconstruct current world position.
2. Project it into the previous view-projection.
3. Sample previous validation buffers.
4. Reuse only when all validation tests pass.
5. Mark failed/disoccluded pixels dirty.

Validation is exact for material ID and uses these initial tolerances:

```text
position_error <= max(0.03 world units, 0.01 * camera_distance)
normal_dot >= 0.94
previous projected UV inside [0,1]
previous depth valid
```

No color-based acceptance test is used.

## 6. Dirty Tile Compaction

Create:

- `Sources/Renderer/Gpu/DirtyTileGpu.hpp`
- `Sources/Renderer/Gpu/DirtyTileGpu.cpp`
- `Sources/Renderer/Gpu/DirtyTileShader.hpp`

Use fixed 8x8 tiles.

After reprojection validation:

- one invalid pixel marks its whole containing tile dirty;
- produce one dirty flag per tile;
- compact dirty tile coordinates into an SSBO using a GPU atomic counter;
- expensive direct/Lumen miss work dispatches only for compacted dirty tiles;
- fully reusable tiles perform no expensive shading dispatch.

This deliberately prefers small amounts of over-shading over incorrect partial-tile reuse.

## 7. Split Static Diffuse From View-Dependent Specular

Create:

- `Sources/Renderer/Gpu/StaticDiffuseLightingGpu.hpp`
- `Sources/Renderer/Gpu/StaticDiffuseLightingGpu.cpp`
- `Sources/Renderer/Gpu/ViewSpecularGpu.hpp`
- `Sources/Renderer/Gpu/ViewSpecularGpu.cpp`

For static lights and static geometry/materials, cache the camera-independent part:

- diffuse BRDF contribution;
- static directional shadow visibility;
- camera-independent emissive/ambient terms.

Update camera-dependent terms during camera motion:

- specular Fresnel/view response;
- reflection weighting;
- transmission/refraction view response.

Dynamic lights continue through the existing dynamic direct-light path.

For a material with both static and dynamic light contributions, the final direct term is the sum of cached static diffuse, current view-dependent specular, and dynamic-light evaluation.

## 8. GPU World-Space Radiance Cache

Create:

- `Sources/Renderer/Gpu/RadianceCacheGpu.hpp`
- `Sources/Renderer/Gpu/RadianceCacheGpu.cpp`
- `Sources/Renderer/Gpu/RadianceCacheShader.hpp`

Purpose: stop tracing imported Sponza triangles repeatedly for GI values that are world-space stable.

Use a sparse hashed 3D probe grid.

Initial design:

- cell size: 0.5 world units;
- hash table: power-of-two capacity, starting at 65536 probe slots;
- key: signed integer 3D cell coordinate plus current radiance-generation revision;
- value: RGB irradiance/radiance, confidence/sample count, and accumulated dominant normal;
- maximum probe samples before considered high confidence: 16;
- cache lookup accepts a probe only when its generation matches and confidence >= 4;
- shading samples the eight neighboring grid cells and blends valid probes by trilinear spatial weights multiplied by normal agreement;
- if no acceptable neighbors exist, trace imported geometry and update the nearest probe slot;
- camera motion never invalidates records;
- geometry/material/light changes increment the radiance generation and logically invalidate all prior probes without clearing the whole buffer synchronously.

Collision handling uses linear probing with a maximum of 8 probes. Failure to insert simply produces an uncached trace for that sample; it must not block rendering.

## 9. Reflection Fallback Cache

Create:

- `Sources/Renderer/Gpu/ReflectionCacheGpu.hpp`
- `Sources/Renderer/Gpu/ReflectionCacheGpu.cpp`

Reflection order is fixed:

1. current screen-space hit;
2. world-space radiance-cache fallback for rough reflections;
3. validated previous reflection history for smooth surfaces;
4. imported triangle trace only when the prior sources miss or fail validation.

History validation requires:

```text
position test: same ReprojectionPolicy threshold
normal_dot >= 0.96
roughness difference <= 0.05
material ID exact match
```

## 10. Imported Ray Geometry Compaction

Create:

- `Sources/Renderer/Gpu/ShadowTriangleGpu.hpp`
- `Sources/Renderer/Gpu/TraceGeometryGpu.hpp`
- `Sources/Renderer/Gpu/TraceGeometryGpu.cpp`

Do not use the same 128-byte `TriangleGpu` record for every ray purpose.

Shadow visibility records contain only:

- 3 positions;
- compact material/flags index needed only for masked/transmission candidates.

GI/reflection records retain positions, UVs, normals, and material identity.

The shadow traversal must not fetch UV/material records for opaque candidates.

## 11. BVH Traversal Improvements

Keep implementation isolated in BVH builder/traversal helpers.

Apply:

- 16-bin SAH split selection for imported BLAS;
- fall back to the existing median/centroid split when SAH produces an empty side or no cost improvement;
- front-to-back child traversal using ray/AABB entry distance;
- separate opaque-only visibility traversal from masked/transmissive traversal;
- early any-hit exit for opaque shadow queries;
- avoid material/UV fetch until a candidate hit actually requires alpha/transmission evaluation;
- preserve TLAS refit for moving transforms;
- never rebuild static BLAS solely because the camera moved.

A contract must compare hit/miss and nearest-hit distance against the pre-optimization traversal on generated deterministic rays.

## 12. GBuffer Bandwidth Reduction

Do not reduce primary image resolution.

Reduce per-pixel memory traffic in two stages.

### Stage A

- stop storing full world position in the long-term GBuffer contract;
- keep depth and reconstruct world position from depth plus inverse view-projection;
- add exact `R32UI` material ID for cache/reprojection validation;
- move material constants that are uniform across a material to the material SSBO;
- retain texture-driven roughness/metallic/specular/transmission values per pixel only when necessary.

### Stage B

After Stage A visual verification:

- evaluate octahedral two-channel normal storage;
- adopt it only if RendererCheck/visual comparisons show no material-normal regression;
- otherwise retain the existing normal precision.

Attachment reduction is incremental. A stage is not accepted merely because it uses less memory; it must preserve rendered behavior.

## 13. Static Scene Simulation Policy

Add an explicit `dynamic` flag to `Renderer::Test::SceneState`.

Normal Sponza gameplay scene:

```text
dynamic = false
```

RendererCheck scenes that intentionally animate lights/objects:

```text
dynamic = true
```

When false, the main loop does not call scene-animation update code. Camera input remains independent and active.

Do not infer scene dynamics from entity count or recent frame history.

## 14. Low-FPS Profiling

Update profiling so explicit profiling is useful even below 1 FPS.

When `CRAPGAME_GPU_PROFILE=1`:

- issue timer queries for every rendered frame;
- aggregate asynchronously;
- print at most once per second when results are available;
- never block waiting for a query result;
- include cache counters.

Required counters:

- geometry ms
- static shadow generation ms / `cached`
- static diffuse/direct ms
- view specular ms
- reprojection ms
- dirty tile count / total tiles
- reused pixel percentage
- Lumen trace ms
- radiance-cache hit percentage
- reflection-cache hit percentage
- composite ms
- present ms

Default non-profile gameplay does not pay CPU readback/query overhead for these statistics.

## 15. Frame Scheduling Rules

Replace time-only Lumen behavior with change/convergence-aware scheduling.

Rules:

- scene/light/material revision change -> immediate invalidation and sample;
- camera change -> use reprojection and bounded refresh; do not globally invalidate world-space static caches;
- continuous camera motion -> at most one expensive secondary-effect refresh every 66,666,667 ns (15 Hz), with intermediate frames using reprojection/cache data;
- unchanged camera + unchanged scene -> converge to the default eight samples then freeze;
- unchanged converged state -> no expensive trace/composite dispatch merely because time passed.

`CRAPGAME_LUMEN_HZ` remains a debugging/performance override. It may alter the moving-camera refresh interval, but it does not disable revision-based invalidation or static-frame convergence freeze unless an explicit debug-force flag is also set.

## 16. Render-Loop Ownership

`Rendering::renderGpuFrame` remains orchestration only.

It decides which subsystem runs based on revision/cache policy and delegates work to focused classes.

It must not accumulate shader source, cache validation math, tile compaction logic, or BVH implementation details.

Expected high-level orchestration:

```text
ScenePrewarm / residency
RevisionState
GBuffer
StaticShadowCacheGpu
ReprojectionCacheGpu
DirtyTileGpu
StaticDiffuseLightingGpu
ViewSpecularGpu
RadianceCacheGpu
ReflectionCacheGpu
Lumen composite
TransparentGpu
ConvergedFrameCache
Presenter
```

## 17. Performance Acceptance

The goal is structural elimination of repeated work rather than a hardware-specific FPS promise.

### Stationary static Sponza

After convergence:

- no GBuffer redraw;
- no direct-light dispatch;
- no imported triangle shadow traversal;
- no imported Lumen trace;
- no composite regeneration;
- only transparent work when transparent state is explicitly dynamic; otherwise only presentation/window work remains.

### Moving camera, static Sponza

- native-resolution raster remains;
- static directional shadow cache remains valid;
- world-space radiance cache remains valid;
- reprojection reuses valid lighting history;
- expensive updates operate on dirty tiles/cache misses rather than the full frame where possible;
- camera movement alone does not rebuild BLAS, trace atlases, static shadows, or radiance generation.

### Scene/light/material change

- affected caches invalidate before reuse;
- no stale lighting/shadows are presented as valid.

### Startup

- shader compilation, texture residency, mesh upload, trace atlases, and static acceleration build occur during prewarm rather than unexpectedly on the first visible frame.

## 18. Testing Strategy

Every subsystem gets a focused failing contract before production implementation.

Required contracts:

- revision invalidation matrix;
- converged-frame freeze/wakeup at 8 samples;
- prewarm calls all required residency paths;
- static directional shadow camera-independence;
- masked-material shadow invalidation;
- reprojection validation/rejection using exact policy tolerances;
- 8x8 dirty-tile compaction;
- static diffuse/view-specular split;
- radiance-cache camera independence, confidence, collision bound, and generation invalidation;
- reflection fallback ordering;
- compact shadow/GI record layout;
- 16-bin SAH traversal semantic equivalence;
- GBuffer position reconstruction/material ID packing;
- static scene simulation skip;
- low-FPS profiling nonblocking cadence;
- stationary Sponza performs no expensive post-convergence work.

Final verification:

- strict C++17 focused contracts with warnings-as-errors;
- `c build`;
- RendererCheck smoke/contract suite;
- interactive Sponza startup;
- GPU profiling capture before/after on available hardware;
- `git diff --check`;
- no new third-party dependency.

## 19. Commit Structure

Implementation is split into small logical commits:

- `Perf: freeze converged static frames`
- `Perf: prewarm imported renderer resources`
- `Perf: cache static directional shadows`
- `Perf: reproject static lighting history`
- `Perf: compact dirty shading tiles`
- `Perf: split static diffuse and view specular`
- `Perf: add world-space radiance cache`
- `Perf: cache reflection fallbacks`
- `Perf: compact imported ray geometry`
- `Perf: improve imported BVH traversal`
- `Perf: reconstruct GBuffer world position`
- `Perf: reduce GBuffer bandwidth`
- `Perf: skip static scene simulation`
- `Perf: expose low-FPS GPU profiling`
- `Perf: verify static-scene cache pipeline`

Each commit must leave `main` buildable or be paired immediately with the exact contract it introduces.
