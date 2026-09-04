# Static-Scene Performance and Caching Design

Date: 2026-09-04
Repository: `xt9y/CrapGame`
Target branch: `main`

## Goal

Make the interactive Sponza scene fast because its geometry, materials, and directional light are static, not by lowering whole-frame resolution or requiring faster hardware.

The renderer must avoid recomputing expensive work whose inputs have not changed. Native primary raster resolution remains the default. Expensive secondary effects become cached, reprojected, tiled, or sparse.

All implementation commits for this work use the prefix `Perf: `.

## Constraints

- Keep the primary GBuffer/raster image at native window resolution.
- Do not introduce third-party rendering dependencies.
- Preserve existing RendererCheck deterministic reference behavior.
- Preserve correctness for dynamic lights, moving geometry, transparent/transmissive materials, masked alpha geometry, and camera motion.
- Prefer focused classes/files over expanding `Render.cpp` or monolithic GPU files.
- Cache validity must be driven by explicit revisions and inputs, not guessed from elapsed time.
- A cache hit must never silently reuse data after one of its semantic inputs changed.

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
  +-- initialize world-space GI/radiance cache
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
  |                +-- sample world-space GI/radiance caches
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
| static directional shadow | yes | alpha/masked | yes | no | resolution/light atlas only |
| world radiance/probes | yes | yes | yes | no | no |
| reprojection history | yes | yes | yes | yes | yes |
| converged final frame | yes | yes | yes | yes | yes |
| static diffuse lighting cache | yes | yes | yes | reprojection only | yes |
| view specular cache | yes | yes | yes | yes | yes |

No cache may key directly on wall-clock time.

## 2. Converged Frame Cache

Create:

- `Sources/Renderer/Gpu/ConvergedFrameCache.hpp`
- `Sources/Renderer/Gpu/ConvergedFrameCache.cpp`

Purpose: stop rendering expensive passes when the exact scene, light, camera, and resolution are unchanged and Lumen history has converged.

State:

- cached final texture handle/reference
- revision tuple
- convergence sample count
- convergence stable-count
- last measured temporal delta/variance
- valid/converged flags

Behavior:

1. Any relevant revision change invalidates convergence.
2. While unconverged, Lumen may produce bounded additional samples.
3. A sample is considered stable when the measured/history delta is below a fixed threshold.
4. After `N` consecutive stable samples, mark converged.
5. Also force convergence after a finite maximum sample count to prevent infinite refinement.
6. Once converged and revisions remain identical, skip geometry, direct, Lumen, and composite work and present the cached final texture.

Initial contract values:

- minimum convergence samples: 4
- stable samples required: 3
- maximum convergence samples: 16

Thresholds must be constants in one policy file/class and covered by tests.

## 3. Scene Prewarm

Create:

- `Sources/Renderer/Gpu/ScenePrewarm.hpp`
- `Sources/Renderer/Gpu/ScenePrewarm.cpp`

Purpose: move one-time residency/compilation work out of the first visible frame.

Prewarm after scene construction and renderer initialization, before the interactive frame loop.

Prewarm performs:

- upload all loaded meshes used by the world;
- upload all referenced material textures;
- build mip chains;
- build imported BLAS/TLAS resources;
- rebuild trace-material GPU records/atlases;
- compile the imported direct-light shader;
- compile the imported Lumen shader;
- allocate GBuffer/direct/Lumen/transparent targets;
- build static directional shadow resources when applicable;
- initialize world-space GI/radiance-cache resources.

The prewarm API returns explicit success/error. Failure must not silently fall back to per-frame lazy initialization.

## 4. Static Directional Shadow Cache

Create:

- `Sources/Renderer/Gpu/StaticShadowCacheGpu.hpp`
- `Sources/Renderer/Gpu/StaticShadowCacheGpu.cpp`
- `Sources/Renderer/Gpu/StaticShadowShader.hpp`

Purpose: eliminate per-screen-pixel imported-triangle shadow traversal for a static directional light.

Use a light-space depth/visibility atlas generated from static scene geometry.

Requirements:

- full support for opaque geometry;
- alpha test for masked materials while generating the cache;
- transparent/transmissive geometry excluded from opaque shadow depth and handled by existing dynamic/transmission logic where required;
- cache keyed by geometry/material/light revisions;
- camera movement never invalidates this cache;
- PCF or equivalent small filtered lookup to avoid hard aliasing;
- dynamic lights and moving geometry retain the existing ray/BVH path.

The normal Sponza directional light must use the cached path after initial generation.

## 5. Reprojection Cache

Create:

- `Sources/Renderer/Gpu/ReprojectionCacheGpu.hpp`
- `Sources/Renderer/Gpu/ReprojectionCacheGpu.cpp`
- `Sources/Renderer/Gpu/ReprojectionShader.hpp`

Purpose: reuse previous expensive lighting when only the camera moves.

History stores enough data to validate reuse:

- previous world position/depth
- previous normal
- previous material identifier or equivalent material validation value
- previous direct diffuse/static lighting
- previous indirect lighting
- previous reflection contribution

For each current pixel:

1. Reconstruct current world position.
2. Project it into the previous view-projection.
3. Sample previous validation buffers.
4. Reuse only when position/depth, normal, and material tests pass.
5. Mark all failed/disoccluded pixels dirty.

Initial validation tolerances must be centralized in a policy helper and tested.

## 6. Dirty Tile Compaction

Create:

- `Sources/Renderer/Gpu/DirtyTileGpu.hpp`
- `Sources/Renderer/Gpu/DirtyTileGpu.cpp`
- `Sources/Renderer/Gpu/DirtyTileShader.hpp`

Use 8x8 tiles.

After reprojection validation:

- produce one dirty flag per tile;
- compact dirty tile coordinates into an SSBO;
- dispatch expensive direct/Lumen miss work only for compacted dirty tiles;
- fully reusable tiles perform no expensive shading dispatch.

Correctness rule: any invalid pixel marks its containing tile dirty. This deliberately prefers some over-shading over incorrect cache reuse.

## 7. Split Static Diffuse From View-Dependent Specular

Create:

- `Sources/Renderer/Gpu/StaticDiffuseLightingGpu.hpp`
- `Sources/Renderer/Gpu/StaticDiffuseLightingGpu.cpp`
- `Sources/Renderer/Gpu/ViewSpecularGpu.hpp`
- `Sources/Renderer/Gpu/ViewSpecularGpu.cpp`

For static lights and static geometry/materials, cache the camera-independent part:

- diffuse BRDF contribution;
- static directional shadow visibility;
- camera-independent emissive/ambient terms where applicable.

Update only camera-dependent terms during camera motion:

- specular Fresnel/view response;
- reflection weighting;
- transmission/refraction view response.

Dynamic lights continue through the normal direct-light path.

## 8. GPU World-Space Radiance Cache

Create:

- `Sources/Renderer/Gpu/RadianceCacheGpu.hpp`
- `Sources/Renderer/Gpu/RadianceCacheGpu.cpp`
- `Sources/Renderer/Gpu/RadianceCacheShader.hpp`

Purpose: stop tracing imported Sponza triangles repeatedly for GI values that are world-space stable.

The cache is world-space and camera-independent.

Design:

- sparse hashed 3D cells or probe records around visible/important geometry;
- each record stores irradiance/radiance, confidence/sample count, and revision stamp;
- primary Lumen shading queries the cache first;
- cache hit -> reuse;
- cache miss/low confidence -> trace imported geometry and update record;
- camera motion does not invalidate records;
- geometry/material/light changes invalidate affected cache generation globally at first implementation, with room for local invalidation later.

The first implementation may use global revision invalidation rather than spatially selective invalidation, but it must not rebuild solely due to camera movement.

## 9. Reflection Fallback Cache

Create:

- `Sources/Renderer/Gpu/ReflectionCacheGpu.hpp`
- `Sources/Renderer/Gpu/ReflectionCacheGpu.cpp`

Reflection order:

1. screen-space hit/current visible information;
2. cached world-space reflection/radiance data;
3. imported triangle trace only for misses.

Temporal validation uses world position, normal, roughness, and material identity.

## 10. Imported Ray Geometry Compaction

Create focused shadow/GI records rather than using the same large triangle representation for every ray purpose.

Add:

- `Sources/Renderer/Gpu/ShadowTriangleGpu.hpp`
- `Sources/Renderer/Gpu/TraceGeometryGpu.hpp`
- `Sources/Renderer/Gpu/TraceGeometryGpu.cpp`

Shadow records contain only data required for visibility/alpha lookup.
GI/reflection records retain UV/normal/material information.

Goals:

- reduce SSBO bandwidth;
- reduce cache pressure;
- keep material-aware tracing only where needed.

## 11. BVH Traversal Improvements

Keep implementation isolated in BVH builder/traversal helpers.

Apply:

- binned SAH split selection for imported BLAS where it improves estimated traversal cost;
- front-to-back child ordering from ray/AABB entry distance;
- separate opaque-only visibility traversal from masked/transmissive traversal;
- early any-hit exit for opaque shadow queries;
- avoid material/UV fetch until a candidate hit actually requires alpha/transmission evaluation;
- preserve TLAS refit for moving transforms;
- avoid per-frame BLAS rebuild for static meshes.

No optimization may change hit semantics.

## 12. GBuffer Bandwidth Reduction

Do not reduce primary image resolution.

Reduce per-pixel memory traffic instead.

First targets:

- reconstruct world position from depth + inverse view-projection instead of storing full world position where practical;
- replace per-pixel uniform material values with material ID + material SSBO lookup when values are constant over the material;
- retain per-pixel values only when texture-driven or interpolation-dependent;
- compact normals where precision remains visually safe;
- reduce attachment count in stages rather than one large rewrite.

Each format change requires source/contract tests and RendererCheck/visual verification.

## 13. Static Scene Simulation Policy

Add an explicit scene dynamics flag to the scene/test state.

Normal Sponza gameplay scene:

```text
dynamic = false
```

RendererCheck scenes that intentionally animate lights/objects:

```text
dynamic = true
```

When false, the main loop must not call scene-animation update code. Camera input remains independent and active.

Do not infer scene dynamics from entity count or frame history.

## 14. Low-FPS Profiling

Create/update a profiling policy so explicit profiling is useful even at 1 FPS.

When `CRAPGAME_GPU_PROFILE=1`:

- query pass timings every rendered frame;
- print no more than approximately once per second;
- include cache statistics.

Required counters:

- geometry ms
- static shadow generation ms / cached marker
- direct/static diffuse ms
- view specular ms
- reprojection ms
- dirty tile count / total tiles
- reused pixel percentage
- Lumen trace ms
- radiance-cache hit percentage
- reflection-cache hit percentage
- composite ms
- present ms

Default non-profile gameplay must not pay CPU readback/query overhead for these statistics.

## 15. Frame Scheduling Rules

Replace time-only Lumen behavior with change/convergence-aware scheduling.

Rules:

- scene/light/material revision change -> immediate invalidation and sample;
- camera change -> use reprojection and bounded refresh; do not globally invalidate world-space static caches;
- continuous camera motion -> bounded secondary-effect update cadence;
- unchanged camera + unchanged scene -> converge for a finite number of samples then freeze;
- unchanged converged state -> no expensive trace/composite dispatch merely because time passed.

A fixed environment override such as `CRAPGAME_LUMEN_HZ` may remain for debugging/performance testing, but default behavior follows cache/convergence state.

## 16. Render-Loop Ownership

`Rendering::renderGpuFrame` should remain orchestration only.

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

Required observable behavior:

### Stationary static Sponza

After convergence:

- no GBuffer redraw;
- no direct-light dispatch;
- no imported triangle shadow traversal;
- no imported Lumen trace;
- no composite regeneration;
- only presentation/window work remains.

### Moving camera, static Sponza

- native-resolution raster remains;
- static directional shadow cache remains valid;
- world-space radiance cache remains valid;
- reprojection reuses valid lighting history;
- expensive updates operate on dirty tiles/cache misses rather than the full frame where possible.

### Scene/light/material change

- affected caches invalidate immediately;
- no stale lighting/shadows are presented as valid.

### Startup

- shader compilation, texture residency, mesh upload, trace atlases, and static acceleration build occur during prewarm rather than unexpectedly on the first visible frame.

## 18. Testing Strategy

Every subsystem gets a focused contract before production implementation.

Required contracts include:

- revision invalidation matrix;
- converged-frame freeze/wakeup;
- prewarm calls all required residency paths;
- static directional shadow camera-independence;
- masked-material shadow invalidation;
- reprojection validation/rejection;
- 8x8 dirty-tile compaction;
- static diffuse/view-specular split;
- radiance-cache camera independence and scene invalidation;
- reflection fallback ordering;
- compact shadow/GI record layout;
- BVH traversal semantic equivalence;
- GBuffer reconstruction/packing;
- static scene simulation skip;
- low-FPS profiling cadence;
- stationary Sponza performs no expensive post-convergence work.

Final verification:

- strict C++17 compile with warnings-as-errors for focused contracts;
- `c build`;
- RendererCheck smoke/contract suite;
- interactive Sponza startup;
- GPU profiling capture before/after on available hardware;
- `git diff --check`;
- no new third-party dependency.

## 19. Commit Structure

Implementation should be split into small logical commits such as:

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
- `Perf: reduce GBuffer bandwidth`
- `Perf: skip static scene simulation`
- `Perf: expose low-FPS GPU profiling`

Each commit must leave `main` buildable or be paired immediately with the exact contract it introduces.
