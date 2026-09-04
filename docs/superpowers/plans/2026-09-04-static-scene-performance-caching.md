# Static-Scene Performance and Caching Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate repeated full-frame shadow, direct-light, and Lumen work for the static Sponza scene while keeping native primary raster resolution and correct invalidation for camera, scene, material, light, and resize changes.

**Architecture:** Add explicit renderer revision domains and focused caches around the existing GPU pipeline. Static geometry/light work becomes persistent; camera motion reuses/reprojects cached lighting and shades only invalid tiles; stationary frames converge once and then present the cached final texture without expensive GPU dispatches. Startup residency and shader compilation move into an explicit prewarm phase.

**Tech Stack:** C++17, OpenGL 4.3 through lwcgl v2.9.3, compute shaders, SSBOs, texture arrays, existing ECS, existing RendererCheck contracts, custom C-BuildSystem.

**Spec:** `docs/superpowers/specs/2026-09-04-static-scene-performance-caching-design.md`

## Global Constraints

- Primary GBuffer/raster resolution remains the native window resolution.
- No new third-party rendering dependency.
- RendererCheck deterministic CPU-reference behavior must remain unchanged.
- Dynamic lights, moving geometry, transparent/transmissive materials, masked alpha geometry, and camera motion must remain correct.
- Cache validity is revision/input driven, never wall-clock guessed.
- Focused `.hpp/.cpp` ownership is preferred over adding large implementation blocks to `Render.cpp`.
- Every implementation commit uses the prefix `Perf: `.
- Work is explicitly authorized on `main` by the user.

---

## File Map

### New policy/cache files

- `Sources/Renderer/Gpu/RevisionState.hpp` — renderer semantic revision tuple and invalidation predicates.
- `Sources/Renderer/Gpu/ConvergedFrameCache.hpp/.cpp` — finite convergence/freeze state.
- `Sources/Renderer/Gpu/ScenePrewarm.hpp/.cpp` — one-time scene residency and shader/cache prewarm orchestration.
- `Sources/Renderer/Gpu/StaticShadowCacheGpu.hpp/.cpp` — cached 2048x2048 directional-light shadow target and revision ownership.
- `Sources/Renderer/Gpu/StaticShadowShader.hpp` — directional shadow generation/sampling shaders.
- `Sources/Renderer/Gpu/ReprojectionCacheGpu.hpp/.cpp` — previous-frame lighting validation/reprojection resources.
- `Sources/Renderer/Gpu/ReprojectionShader.hpp` — reprojection/validity compute shader.
- `Sources/Renderer/Gpu/DirtyTileGpu.hpp/.cpp` — 8x8 dirty tile mask/compaction buffers.
- `Sources/Renderer/Gpu/DirtyTileShader.hpp` — dirty tile marking/compaction shader.
- `Sources/Renderer/Gpu/StaticDiffuseLightingGpu.hpp/.cpp` — camera-independent cached direct diffuse/emissive/ambient contribution.
- `Sources/Renderer/Gpu/ViewSpecularGpu.hpp/.cpp` — view-dependent direct specular/reflection/transmission response.
- `Sources/Renderer/Gpu/RadianceCacheGpu.hpp/.cpp` — sparse world-space hashed irradiance/radiance records.
- `Sources/Renderer/Gpu/RadianceCacheShader.hpp` — cache lookup/update helpers.
- `Sources/Renderer/Gpu/ReflectionCacheGpu.hpp/.cpp` — screen/cache/ray fallback ordering and temporal reuse.
- `Sources/Renderer/Gpu/ShadowTriangleGpu.hpp` — compact shadow visibility record layout.
- `Sources/Renderer/Gpu/TraceGeometryGpu.hpp/.cpp` — separate compact visibility and material-aware trace geometry buffers.
- `Sources/Renderer/Gpu/GBufferReconstruct.hpp` — depth/world-position reconstruction helpers and packing policy.
- `Sources/Renderer/Gpu/CacheStats.hpp` — cache/profiler counters with zero-overhead disabled path.

### Existing files modified

- `Sources/Renderer/Render.hpp/.cpp` — orchestration only; owns new cache objects and revision state.
- `Sources/Renderer/Gpu/LumenSchedule.hpp` — convergence/change-aware scheduling rather than endless time-only updates.
- `Sources/Renderer/Gpu/LumenGpu.hpp/.cpp` and imported Lumen source — integrate radiance/reflection cache and sparse dirty work.
- `Sources/Renderer/Gpu/DirectLightingGpu.hpp/.cpp`, `DirectLightingScene.cpp`, `DirectLightingImported.cpp` — static shadow/static diffuse/view specular split.
- `Sources/Renderer/Gpu/TriangleScene.hpp/.cpp` — expose prewarm/static/dynamic revisions and compact trace geometry ownership.
- `Sources/Renderer/Gpu/GBufferGpu.hpp/.cpp` — reduced bandwidth and reconstruction support.
- `Sources/Renderer/Gpu/Bvh.hpp/.cpp` and imported traversal shader source — 16-bin SAH/front-to-back/opaque any-hit improvements.
- `Sources/Renderer/Gpu/Profiler.hpp/.cpp` — explicit low-FPS every-frame query policy and cache counters.
- `Sources/Renderer/Test/TestScene.hpp/.cpp` — explicit `SceneState::dynamic` flag.
- `Sources/main.cpp` — skip scene simulation when `dynamic == false`, invoke prewarm before visible frame loop.

---

### Task 1: Semantic Revision State and Converged Static-Frame Freeze

**Files:**
- Create: `Sources/Renderer/Gpu/RevisionState.hpp`
- Create: `Sources/Renderer/Gpu/ConvergedFrameCache.hpp`
- Create: `Sources/Renderer/Gpu/ConvergedFrameCache.cpp`
- Modify: `Sources/Renderer/Gpu/LumenSchedule.hpp`
- Modify: `Sources/Renderer/Render.hpp`
- Modify: `Sources/Renderer/Render.cpp`
- Test: `tests/static_cache_revision_contract.cpp`
- Test: `tests/converged_frame_cache_contract.cpp`

**Interfaces:**
- Produces:
```cpp
struct RevisionState {
    std::uint64_t geometry = 0;
    std::uint64_t material = 0;
    std::uint64_t lighting = 0;
    std::uint64_t camera = 0;
    std::uint64_t resolution = 0;
    std::uint64_t mesh_registry = 0;
    std::uint64_t material_registry = 0;
};

class ConvergedFrameCache {
public:
    void invalidate();
    void observe(const RevisionState& revisions, float temporal_delta);
    bool needsSample(const RevisionState& revisions) const;
    bool frozen(const RevisionState& revisions) const;
    void reset();
};
```
- Constants: minimum samples `4`, stable samples `3`, hard maximum samples `8` for the normal static scene.

- [ ] **Step 1: Write failing revision/freeze contracts**

```cpp
int main() {
    using namespace Renderer::Gpu;
    RevisionState r{};
    ConvergedFrameCache cache;
    require(cache.needsSample(r), "first frame samples");
    for (int i=0;i<8;++i) cache.observe(r, 0.0f);
    require(cache.frozen(r), "static frame freezes after bounded convergence");
    RevisionState moved=r; ++moved.camera;
    require(!cache.frozen(moved), "camera revision wakes frame");
    RevisionState relit=r; ++relit.lighting;
    require(!cache.frozen(relit), "light revision wakes frame");
}
```

- [ ] **Step 2: Run contracts and verify RED**

Run:
```bash
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -ISources tests/static_cache_revision_contract.cpp -o /tmp/static-cache-revision
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -ISources tests/converged_frame_cache_contract.cpp Sources/Renderer/Gpu/ConvergedFrameCache.cpp -o /tmp/converged-cache
```
Expected: missing header/type/symbol failures.

- [ ] **Step 3: Implement revision predicates and convergence state**

`RevisionState.hpp` must provide exact helpers:
```cpp
bool sameSceneLighting(const RevisionState& a,const RevisionState& b);
bool sameFrameInputs(const RevisionState& a,const RevisionState& b);
bool staticShadowValid(const RevisionState& cached,const RevisionState& current);
bool worldRadianceValid(const RevisionState& cached,const RevisionState& current);
```
`sameFrameInputs` includes camera+resolution; `staticShadowValid` excludes camera; `worldRadianceValid` excludes camera+resolution.

- [ ] **Step 4: Integrate freeze into `renderGpuFrame`**

Before scheduling geometry/direct/Lumen, construct the current semantic revision tuple and:
```cpp
if (converged_frame_cache_.frozen(gpu_revisions_)) {
    return presenter_.presentTexture(gpu_lumen_.finalTexture(), &error);
}
```
After each actual Lumen sample call `observe(...)`. Never schedule a new default Lumen sample solely because wall-clock time elapsed after freeze. Keep `CRAPGAME_LUMEN_HZ` as an explicit debug override.

- [ ] **Step 5: Run focused contracts**
Expected: both PASS.

- [ ] **Step 6: Commit**
```bash
git commit -m "Perf: freeze converged static frames"
```

---

### Task 2: Explicit Scene Prewarm

**Files:**
- Create: `Sources/Renderer/Gpu/ScenePrewarm.hpp`
- Create: `Sources/Renderer/Gpu/ScenePrewarm.cpp`
- Modify: `Sources/Renderer/Render.hpp`
- Modify: `Sources/Renderer/Render.cpp`
- Modify: `Sources/main.cpp`
- Modify: `Sources/Renderer/Gpu/GBufferGpu.hpp/.cpp`
- Modify: `Sources/Renderer/Gpu/DirectLightingGpu.hpp/.cpp`
- Modify: `Sources/Renderer/Gpu/LumenGpu.hpp/.cpp`
- Modify: `Sources/Renderer/Gpu/TriangleScene.hpp/.cpp`
- Test: `tests/scene_prewarm_contract.cpp`

**Interfaces:**
```cpp
class ScenePrewarm {
public:
    bool run(const Ecs::World& world,
             GBufferGpu& gbuffer,
             DirectLightingGpu& direct,
             LumenGpu& lumen,
             int width,int height,
             std::string* error=nullptr);
    bool complete() const;
};
```
Each subsystem exposes idempotent `prewarm(...)`/`ensureResident(...)` entry points that perform existing lazy work without rendering a visible frame.

- [ ] **Step 1: Write failing prewarm contract** using fake subsystem probes that record `ensureResident`, imported scene build, trace material atlas build, shader precompile, and resize allocation calls.
- [ ] **Step 2: Verify RED** because `ScenePrewarm` does not exist.
- [ ] **Step 3: Implement `ScenePrewarm`** so failure is explicit and no lazy fallback is silently accepted.
- [ ] **Step 4: Invoke prewarm after `buildScene`, renderer init, and resize but before entering the visible frame loop.**
- [ ] **Step 5: Verify contract + strict compile.**
- [ ] **Step 6: Commit**
```bash
git commit -m "Perf: prewarm imported renderer resources"
```

---

### Task 3: Static Directional Shadow Cache

**Files:**
- Create: `Sources/Renderer/Gpu/StaticShadowCacheGpu.hpp`
- Create: `Sources/Renderer/Gpu/StaticShadowCacheGpu.cpp`
- Create: `Sources/Renderer/Gpu/StaticShadowShader.hpp`
- Modify: `Sources/Renderer/Gpu/DirectLightingGpu.hpp/.cpp`
- Modify: `Sources/Renderer/Gpu/DirectLightingScene.cpp`
- Modify: `Sources/Renderer/Gpu/DirectLightingImported.cpp`
- Test: `tests/static_shadow_cache_contract.cpp`

**Interfaces:**
```cpp
class StaticShadowCacheGpu {
public:
    bool init(std::string* error=nullptr);
    bool ensure(const Ecs::World& world,const TriangleScene& triangles,
                const RevisionState& revisions,std::string* error=nullptr);
    bool validFor(const RevisionState& revisions) const;
    GLuint depthTexture() const;
    const Math::Mat4& lightViewProjection() const;
    void shutdown();
};
```
- Fixed atlas: `2048x2048`, depth texture, 3x3 PCF in the consumer.
- Camera revision is excluded from validity.
- Masked materials alpha-test while generating the atlas.

- [ ] **Step 1: Write failing contract** proving camera-only revisions preserve the cache and geometry/material/light revisions invalidate it.
- [ ] **Step 2: Verify RED.**
- [ ] **Step 3: Implement light-frustum fitting and cached atlas generation** in the new class/shader files; keep transparent/transmissive surfaces out of opaque depth.
- [ ] **Step 4: Change directional-light shading** to use static cache visibility when valid; retain current ray/BVH path for dynamic light/geometry/transmission cases.
- [ ] **Step 5: Verify source contract and strict build.**
- [ ] **Step 6: Commit**
```bash
git commit -m "Perf: cache static directional shadows"
```

---

### Task 4: Reprojection History Cache

**Files:**
- Create: `Sources/Renderer/Gpu/ReprojectionCacheGpu.hpp/.cpp`
- Create: `Sources/Renderer/Gpu/ReprojectionShader.hpp`
- Modify: `Sources/Renderer/Render.hpp/.cpp`
- Modify: `Sources/Renderer/Gpu/LumenGpu.hpp/.cpp`
- Test: `tests/reprojection_cache_contract.cpp`

**Interfaces:**
```cpp
struct ReprojectionPolicy {
    static constexpr float POSITION_EPSILON = 0.08f;
    static constexpr float NORMAL_DOT_MIN = 0.97f;
    static constexpr float DEPTH_RELATIVE_EPSILON = 0.015f;
};

class ReprojectionCacheGpu {
public:
    bool resize(int width,int height,std::string* error=nullptr);
    bool reproject(const GBufferGpu& current,const Math::Mat4& previous_vp,
                   const Math::Mat4& current_inverse_vp,std::string* error=nullptr);
    GLuint validMask() const;
    GLuint reusedLighting() const;
    void commitHistory(...);
    void invalidate();
};
```

- [ ] **Step 1: Write failing pure validation contract** for accepted/rejected position/normal/depth/material cases.
- [ ] **Step 2: Verify RED.**
- [ ] **Step 3: Implement resource/history lifecycle and shader validation.**
- [ ] **Step 4: Integrate camera-only path** so static world caches remain valid and previous lighting is reprojected before expensive shading.
- [ ] **Step 5: Verify.**
- [ ] **Step 6: Commit**
```bash
git commit -m "Perf: reproject static lighting history"
```

---

### Task 5: Dirty 8x8 Tile Compaction

**Files:**
- Create: `Sources/Renderer/Gpu/DirtyTileGpu.hpp/.cpp`
- Create: `Sources/Renderer/Gpu/DirtyTileShader.hpp`
- Modify: `Sources/Renderer/Render.hpp/.cpp`
- Test: `tests/dirty_tile_contract.cpp`

**Interfaces:**
```cpp
class DirtyTileGpu {
public:
    static constexpr int TILE_SIZE=8;
    bool resize(int width,int height,std::string* error=nullptr);
    bool compact(GLuint valid_mask,std::string* error=nullptr);
    GLuint tileBuffer() const;
    std::uint32_t dirtyCount() const;
    std::uint32_t totalCount() const;
};
```
- Any invalid pixel marks its containing tile dirty.

- [ ] **Step 1: Write failing CPU policy contract** for tile indexing/whole-tile dirty semantics.
- [ ] **Step 2: Verify RED.**
- [ ] **Step 3: Implement tile mask + compaction buffers.**
- [ ] **Step 4: Expose dirty tile list to direct/Lumen miss dispatches.**
- [ ] **Step 5: Verify.**
- [ ] **Step 6: Commit**
```bash
git commit -m "Perf: compact dirty shading tiles"
```

---

### Task 6: Split Static Diffuse and View-Dependent Specular

**Files:**
- Create: `Sources/Renderer/Gpu/StaticDiffuseLightingGpu.hpp/.cpp`
- Create: `Sources/Renderer/Gpu/ViewSpecularGpu.hpp/.cpp`
- Modify: `Sources/Renderer/Gpu/DirectLightingGpu.hpp/.cpp`
- Modify: `Sources/Renderer/Gpu/DirectLightingScene.cpp`
- Modify: `Sources/Renderer/Render.hpp/.cpp`
- Test: `tests/static_direct_split_contract.cpp`

**Interfaces:**
```cpp
class StaticDiffuseLightingGpu {
public:
    bool updateIfNeeded(const GBufferGpu&,const StaticShadowCacheGpu&,
                        const RevisionState&,std::string* error=nullptr);
    GLuint texture() const;
};
class ViewSpecularGpu {
public:
    bool render(const GBufferGpu&,GLuint static_diffuse,const Math::Vec3& camera,
                const DirtyTileGpu*,std::string* error=nullptr);
    GLuint texture() const;
};
```

- [ ] **Step 1: Write failing contract** proving camera changes do not invalidate static diffuse, but do require view specular.
- [ ] **Step 2: Verify RED.**
- [ ] **Step 3: Move camera-independent diffuse/emissive/ambient/static-shadow contribution into `StaticDiffuseLightingGpu`.**
- [ ] **Step 4: Keep Fresnel/specular/reflection/transmission in `ViewSpecularGpu`; dynamic lights remain in the existing dynamic path.**
- [ ] **Step 5: Verify visual/source contracts.**
- [ ] **Step 6: Commit**
```bash
git commit -m "Perf: split cached diffuse and view specular"
```

---

### Task 7: Sparse World-Space Radiance Cache

**Files:**
- Create: `Sources/Renderer/Gpu/RadianceCacheGpu.hpp/.cpp`
- Create: `Sources/Renderer/Gpu/RadianceCacheShader.hpp`
- Modify: `Sources/Renderer/Gpu/LumenGpu.hpp/.cpp`
- Modify: imported Lumen shader source
- Test: `tests/radiance_cache_gpu_contract.cpp`

**Interfaces:**
```cpp
struct RadianceCachePolicy {
    static constexpr float CELL_SIZE=0.5f;
    static constexpr std::uint32_t INITIAL_CAPACITY=65536u;
    static constexpr std::uint32_t MAX_PROBES=8u;
};
class RadianceCacheGpu {
public:
    bool init(std::string* error=nullptr);
    bool beginGeneration(const RevisionState&,std::string* error=nullptr);
    GLuint recordsBuffer() const;
    std::uint64_t generation() const;
    void shutdown();
};
```
Records store quantized/hashed cell key, radiance/irradiance, confidence/sample count, generation stamp. Camera revisions do not advance generation; geometry/material/light revisions do.

- [ ] **Step 1: Write failing hash/generation contract.**
- [ ] **Step 2: Verify RED.**
- [ ] **Step 3: Implement sparse hashed records and bounded 8-probe lookup.**
- [ ] **Step 4: Modify imported Lumen trace** to query cache before triangle tracing and update records on misses/low-confidence hits.
- [ ] **Step 5: Verify camera independence and scene invalidation.**
- [ ] **Step 6: Commit**
```bash
git commit -m "Perf: add static GPU radiance cache"
```

---

### Task 8: Reflection Fallback Cache

**Files:**
- Create: `Sources/Renderer/Gpu/ReflectionCacheGpu.hpp/.cpp`
- Modify: `Sources/Renderer/Gpu/LumenGpu.hpp/.cpp`
- Test: `tests/reflection_cache_gpu_contract.cpp`

**Interfaces:**
```cpp
enum class ReflectionSource { Screen, Cache, Ray };
ReflectionSource chooseReflectionSource(bool screen_hit,bool cache_valid);
```
Order is strictly screen-space/current visible data -> cache -> imported triangle ray.

- [ ] **Step 1: Write failing fallback-order and invalidation contract.**
- [ ] **Step 2: Verify RED.**
- [ ] **Step 3: Implement reflection history/cache validity using world position, normal, roughness, material identity.**
- [ ] **Step 4: Integrate fallback order into Lumen reflection path.**
- [ ] **Step 5: Verify.**
- [ ] **Step 6: Commit**
```bash
git commit -m "Perf: cache reflection fallbacks"
```

---

### Task 9: Compact Imported Ray Geometry

**Files:**
- Create: `Sources/Renderer/Gpu/ShadowTriangleGpu.hpp`
- Create: `Sources/Renderer/Gpu/TraceGeometryGpu.hpp/.cpp`
- Modify: `Sources/Renderer/Gpu/TriangleScene.hpp/.cpp`
- Modify: direct imported-shadow shader source
- Modify: imported Lumen shader source
- Test: `tests/trace_geometry_layout_contract.cpp`

**Interfaces:**
```cpp
struct ShadowTriangleGpu {
    float p0[4], p1[4], p2[4];
    float uv0_uv1[4];
    float uv2_material[4];
};
class TraceGeometryGpu {
public:
    bool rebuildFrom(const TriangleScene& source,std::string* error=nullptr);
    GLuint shadowTriangleBuffer() const;
    GLuint giTriangleBuffer() const;
};
```
Shadow traversal must not fetch normal/material payload unless the candidate needs mask/transmission evaluation.

- [ ] **Step 1: Write failing `sizeof`/layout/semantic contract.**
- [ ] **Step 2: Verify RED.**
- [ ] **Step 3: Build compact shadow and material-aware GI buffers once per static geometry revision.**
- [ ] **Step 4: Switch shadow/Lumen traversal bindings to the appropriate compact buffers.**
- [ ] **Step 5: Verify identical hit semantics with existing contract corpus.**
- [ ] **Step 6: Commit**
```bash
git commit -m "Perf: compact imported ray geometry"
```

---

### Task 10: Imported BVH Traversal Improvements

**Files:**
- Modify: `Sources/Renderer/Gpu/Bvh.hpp`
- Modify: `Sources/Renderer/Gpu/Bvh.cpp`
- Modify: imported shadow/Lumen traversal shader sources
- Test: `tests/bvh_sah_contract.cpp`
- Test: `tests/bvh_semantic_equivalence_contract.cpp`

**Interfaces:**
```cpp
BvhBuild buildBvhSah(const std::vector<BvhBoundsInput>& bounds,
                     std::size_t leaf_size=3u,
                     std::size_t bin_count=16u);
```

- [ ] **Step 1: Write deterministic SAH build contract** validating leaf coverage and stable bounds.
- [ ] **Step 2: Write semantic-equivalence rays** comparing SAH traversal result against brute-force triangle hits.
- [ ] **Step 3: Verify RED.**
- [ ] **Step 4: Implement 16-bin SAH split selection with median fallback for degenerate centroids.**
- [ ] **Step 5: Update GPU traversal** to evaluate child AABB entry distances and visit nearest child first; opaque visibility exits on first valid hit; UV/material fetch remains deferred.
- [ ] **Step 6: Run equivalence contracts.**
- [ ] **Step 7: Commit**
```bash
git commit -m "Perf: improve imported BVH traversal"
```

---

### Task 11: GBuffer Bandwidth Reduction

**Files:**
- Create: `Sources/Renderer/Gpu/GBufferReconstruct.hpp`
- Modify: `Sources/Renderer/Gpu/GBufferGpu.hpp/.cpp`
- Modify: direct/Lumen shaders consuming world position/material constants
- Modify: `Sources/Renderer/Gpu/SurfaceFormats.hpp`
- Test: `tests/gbuffer_reconstruct_contract.cpp`
- Test: `tests/gbuffer_bandwidth_contract.cpp`

**Interfaces:**
```cpp
Math::Vec3 reconstructWorldPosition(float depth,const Math::Vec2& uv,
                                    const Math::Mat4& inverse_view_projection);
```
First safe reduction: stop treating full world-position RGBA16F as mandatory for downstream consumers when depth reconstruction is available. Move uniform material constants to material records/ID lookups while retaining texture-varying values per pixel.

- [ ] **Step 1: Write reconstruction round-trip contract.**
- [ ] **Step 2: Write attachment-byte-count contract proving reduction.**
- [ ] **Step 3: Verify RED.**
- [ ] **Step 4: Implement reconstruction helper and staged attachment reduction.**
- [ ] **Step 5: Update direct/Lumen/reprojection consumers.**
- [ ] **Step 6: Run material/GBuffer contracts.**
- [ ] **Step 7: Commit**
```bash
git commit -m "Perf: reduce GBuffer bandwidth"
```

---

### Task 12: Static Scene Simulation Policy

**Files:**
- Modify: `Sources/Renderer/Test/TestScene.hpp`
- Modify: `Sources/Renderer/Test/TestScene.cpp`
- Modify: `Sources/main.cpp`
- Test: `tests/static_scene_simulation_contract.cpp`

**Interfaces:**
```cpp
struct SceneState {
    // existing fields
    bool dynamic = true;
};
```
Normal interactive Sponza sets `dynamic=false`. RendererCheck/moving test scenes retain `true` where animation is intended.

- [ ] **Step 1: Write failing contract** proving normal Sponza is static and moving-light/object tests are dynamic.
- [ ] **Step 2: Verify RED.**
- [ ] **Step 3: Set scene dynamics explicitly in `buildScene`.**
- [ ] **Step 4: Guard simulation update loop:**
```cpp
if (!renderercheck_mode && !performance_static_scene && scene_state.dynamic) {
    // fixed-step Test::updateScene loop
}
```
Camera update remains independent.
- [ ] **Step 5: Verify.**
- [ ] **Step 6: Commit**
```bash
git commit -m "Perf: skip static scene simulation"
```

---

### Task 13: Low-FPS GPU Profiling and Cache Statistics

**Files:**
- Create: `Sources/Renderer/Gpu/CacheStats.hpp`
- Modify: `Sources/Renderer/Gpu/Profiler.hpp/.cpp`
- Modify: cache subsystems to expose counters only when profiling enabled
- Test: `tests/low_fps_profiler_contract.cpp`

**Interfaces:**
```cpp
struct CacheStats {
    std::uint32_t dirty_tiles=0,total_tiles=0;
    std::uint64_t reused_pixels=0,total_pixels=0;
    std::uint64_t radiance_hits=0,radiance_queries=0;
    std::uint64_t reflection_hits=0,reflection_queries=0;
    bool static_shadow_cached=false;
};
```
When `CRAPGAME_GPU_PROFILE=1`, submit/collect timings every rendered frame but print at most once per ~1 second. Query collection remains nonblocking by default; if a slot is not ready, skip it rather than stall the frame.

- [ ] **Step 1: Write failing cadence/stat formatting policy contract.**
- [ ] **Step 2: Verify RED.**
- [ ] **Step 3: Extend pass enum** with StaticShadow, Reprojection, StaticDiffuse, ViewSpecular/Radiance as needed while keeping performance mode writer compatible.
- [ ] **Step 4: Print compact once-per-second breakdown + cache percentages.**
- [ ] **Step 5: Verify normal gameplay without env flag does not execute cache readback/query formatting.**
- [ ] **Step 6: Commit**
```bash
git commit -m "Perf: expose low-FPS GPU profiling"
```

---

### Task 14: End-to-End Static Sponza Scheduling and Final Verification

**Files:**
- Modify: `Sources/Renderer/Render.hpp/.cpp`
- Modify: `.github/workflows/performance-metrics-contract.yml` only if a final contract entry is necessary; do not enable unnecessary workflow triggers.
- Test: `tests/static_sponza_frame_policy_contract.cpp`

**Required stationary behavior after convergence:**
```cpp
FrameWork work = decideFrameWork(static_revisions, static_revisions,
                                 /*camera_changed=*/false,
                                 /*converged=*/true);
require(!work.geometry);
require(!work.direct);
require(!work.shadow);
require(!work.lumen_trace);
require(!work.composite);
require(work.present);
```

**Required camera-motion behavior:** static shadow and world radiance caches remain valid; native GBuffer raster occurs; reprojection/dirty-tile/view-dependent work runs; only cache misses use expensive tracing.

- [ ] **Step 1: Write failing end-to-end frame-policy contract.**
- [ ] **Step 2: Verify RED.**
- [ ] **Step 3: Centralize orchestration decision in a small pure helper (`FrameWork` / `decideFrameWork`) and make `renderGpuFrame` delegate according to it.**
- [ ] **Step 4: Run every focused performance/material contract.**
- [ ] **Step 5: Run strict build:**
```bash
c build
```
- [ ] **Step 6: Run interactive Sponza with profiling where a GL4.3-capable runner/display is available:**
```bash
CRAPGAME_GPU_PROFILE=1 c build run
```
Record geometry/shadow/direct/reprojection/Lumen/composite/present timings and cache hit percentages. Do not invent FPS improvement if no comparable hardware run exists.
- [ ] **Step 7: Run RendererCheck smoke/contract suite.** Missing approved visual baselines may remain non-approved, but process crashes, validation errors, timeouts, or contract failures are failures.
- [ ] **Step 8: Final repository audit:**
```bash
git diff --check
git status --short
grep -R -n -E 'stb_image|tinyobj|assimp' Sources || true
```
Expected: no whitespace errors, clean tree after commit, no new third-party decoder/loader dependency.
- [ ] **Step 9: Commit final orchestration/verification changes:**
```bash
git commit -m "Perf: finish static scene caching path"
```
