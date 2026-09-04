# Static-Scene Performance and Caching Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate repeated full-frame shadow, direct-light, and imported-Lumen work for the static Sponza scene while preserving native primary raster resolution and exact cache invalidation.

**Architecture:** Add explicit semantic revision domains and focused GPU caches around the existing renderer. Static geometry/light work becomes persistent; camera motion uses native-resolution raster plus reprojection/dirty tiles and cached world-space lighting; stationary frames converge to eight samples by default and then freeze until an actual semantic input changes. One-time shader compilation, texture/mesh residency, trace atlases, and acceleration builds move into an explicit prewarm phase.

**Tech Stack:** C++17, OpenGL 4.3 through lwcgl v2.9.3, compute shaders, SSBOs, texture arrays, existing ECS, custom C-BuildSystem, RendererCheck.

**Spec:** `docs/superpowers/specs/2026-09-04-static-scene-performance-caching-design.md`

## Global Constraints

- Keep the primary GBuffer/raster image at native window resolution.
- Do not introduce third-party rendering dependencies.
- Preserve deterministic RendererCheck CPU-reference behavior.
- Preserve correctness for dynamic lights, moving geometry, transparent/transmissive materials, masked alpha geometry, and camera motion.
- Cache validity is driven by semantic revisions/inputs, never wall-clock time alone.
- No CPU readback or GPU synchronization is added to the normal gameplay hot path for cache decisions.
- `Rendering::renderGpuFrame` remains orchestration only; substantial logic lives in focused classes/files.
- Every implementation commit uses the prefix `Perf: `.
- The user explicitly authorized implementation on `main`.

---

## File Map

**Create:**
- `Sources/Renderer/Gpu/RevisionState.hpp`
- `Sources/Renderer/Gpu/ConvergencePolicy.hpp`
- `Sources/Renderer/Gpu/ConvergedFrameCache.hpp/.cpp`
- `Sources/Renderer/Gpu/ScenePrewarm.hpp/.cpp`
- `Sources/Renderer/Gpu/StaticShadowCacheGpu.hpp/.cpp`
- `Sources/Renderer/Gpu/StaticShadowShader.hpp`
- `Sources/Renderer/Gpu/ReprojectionPolicy.hpp`
- `Sources/Renderer/Gpu/ReprojectionCacheGpu.hpp/.cpp`
- `Sources/Renderer/Gpu/ReprojectionShader.hpp`
- `Sources/Renderer/Gpu/DirtyTileGpu.hpp/.cpp`
- `Sources/Renderer/Gpu/DirtyTileShader.hpp`
- `Sources/Renderer/Gpu/StaticDiffuseLightingGpu.hpp/.cpp`
- `Sources/Renderer/Gpu/ViewSpecularGpu.hpp/.cpp`
- `Sources/Renderer/Gpu/RadianceCacheGpu.hpp/.cpp`
- `Sources/Renderer/Gpu/RadianceCacheShader.hpp`
- `Sources/Renderer/Gpu/ReflectionCacheGpu.hpp/.cpp`
- `Sources/Renderer/Gpu/ShadowTriangleGpu.hpp`
- `Sources/Renderer/Gpu/TraceGeometryGpu.hpp/.cpp`
- `Sources/Renderer/Gpu/GBufferReconstruct.hpp`
- `Sources/Renderer/Gpu/CacheStats.hpp`
- focused contracts under `tests/` named by task below.

**Modify:**
- `Sources/Renderer/Render.hpp/.cpp`
- `Sources/main.cpp`
- `Sources/Renderer/Test/TestScene.hpp/.cpp`
- `Sources/Renderer/Gpu/LumenSchedule.hpp`
- `Sources/Renderer/Gpu/LumenGpu.hpp/.cpp` and imported-Lumen shader source
- `Sources/Renderer/Gpu/DirectLightingGpu.hpp/.cpp`
- `Sources/Renderer/Gpu/DirectLightingScene.cpp`
- `Sources/Renderer/Gpu/DirectLightingImported.cpp`
- `Sources/Renderer/Gpu/TriangleScene.hpp/.cpp`
- `Sources/Renderer/Gpu/GBufferGpu.hpp/.cpp`
- `Sources/Renderer/Gpu/Bvh.hpp/.cpp`
- `Sources/Renderer/Gpu/SurfaceFormats.hpp`
- `Sources/Renderer/Gpu/Profiler.hpp/.cpp`

---

### Task 1: Semantic Revision State + Bounded Static Convergence

**Files:**
- Create: `Sources/Renderer/Gpu/RevisionState.hpp`
- Create: `Sources/Renderer/Gpu/ConvergencePolicy.hpp`
- Create: `Sources/Renderer/Gpu/ConvergedFrameCache.hpp/.cpp`
- Modify: `Sources/Renderer/Gpu/LumenSchedule.hpp`
- Modify: `Sources/Renderer/Render.hpp/.cpp`
- Test: `tests/static_cache_revision_contract.cpp`
- Test: `tests/converged_frame_cache_contract.cpp`

**Produces:**
```cpp
struct RevisionState {
    std::uint64_t geometry=0, material=0, lighting=0, camera=0;
    std::uint64_t resolution=0, mesh_registry=0, material_registry=0;
};

bool sameFrameInputs(const RevisionState&,const RevisionState&);
bool staticShadowValid(const RevisionState&,const RevisionState&);
bool worldRadianceValid(const RevisionState&,const RevisionState&);

struct ConvergencePolicy {
    static constexpr std::uint32_t MIN_SAMPLES=4u;
    static constexpr std::uint32_t DEFAULT_SAMPLES=8u;
    static constexpr std::uint32_t MAX_SAMPLES=16u;
};

class ConvergedFrameCache {
public:
    void invalidate();
    void reset();
    void begin(const RevisionState&);
    void recordSample(const RevisionState&, bool history_refinement_requested);
    bool needsSample(const RevisionState&) const;
    bool frozen(const RevisionState&) const;
    std::uint32_t sampleCount() const;
};
```

- [ ] **Step 1: Write RED contracts.**
```cpp
RevisionState r{};
ConvergedFrameCache c;
require(c.needsSample(r),"first sample required");
for(int i=0;i<8;++i)c.recordSample(r,true);
require(c.frozen(r),"default static convergence freezes at eight");
RevisionState moved=r; ++moved.camera;
require(!c.frozen(moved),"camera invalidates final-frame freeze");
RevisionState relit=r; ++relit.lighting;
require(!c.frozen(relit),"lighting invalidates final-frame freeze");
require(staticShadowValid(r,moved),"camera does not invalidate static shadow");
require(worldRadianceValid(r,moved),"camera does not invalidate world radiance");
```
- [ ] **Step 2: Run strict C++17 contracts and confirm missing API/symbol RED.**
```bash
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -ISources tests/static_cache_revision_contract.cpp -o /tmp/static-cache-revision
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -ISources tests/converged_frame_cache_contract.cpp Sources/Renderer/Gpu/ConvergedFrameCache.cpp -o /tmp/converged-cache
```
- [ ] **Step 3: Implement exact invalidation matrix.** `staticShadowValid` ignores camera/resolution; `worldRadianceValid` ignores camera/resolution; `sameFrameInputs` includes scene/material/light/camera/resolution and both registries.
- [ ] **Step 4: Replace endless default time-only refinement.** First 4 post-invalidation samples always run; samples 5-8 run only through existing history-refinement path; default freezes at 8; explicit debug count is clamped 1-16; 16 is absolute maximum. No GPU variance readback is added.
- [ ] **Step 5: In `renderGpuFrame`, present the already-final texture immediately when `frozen(current_revisions)` is true.** `CRAPGAME_LUMEN_HZ` may affect moving-camera cadence but may not defeat static freeze without an explicit force-refinement debug flag.
- [ ] **Step 6: Run contracts; expect PASS.**
- [ ] **Step 7: Commit.**
```bash
git commit -m "Perf: freeze converged static frames"
```

---

### Task 2: Explicit Scene Prewarm

**Files:**
- Create: `Sources/Renderer/Gpu/ScenePrewarm.hpp/.cpp`
- Modify: renderer/GBuffer/direct/Lumen/TriangleScene interfaces as needed
- Modify: `Sources/main.cpp`
- Test: `tests/scene_prewarm_contract.cpp`

**Produces:**
```cpp
class ScenePrewarm {
public:
    bool run(const Ecs::World&,GBufferGpu&,DirectLightingGpu&,LumenGpu&,
             int width,int height,std::string* error=nullptr);
    bool complete() const;
};
```
- [ ] **Step 1: Write RED fake-subsystem contract** recording calls for mesh residency, material textures, mip generation, BLAS/TLAS, trace records/atlases, imported direct shader, imported Lumen shader, targets, shadow resources, and radiance-cache allocation.
- [ ] **Step 2: Verify RED.**
- [ ] **Step 3: Add idempotent `prewarm/ensureResident` entry points to existing GPU owners; do not duplicate resource ownership.**
- [ ] **Step 4: Implement `ScenePrewarm::run`; failure returns false with error and may not silently defer work to first visible frame.**
- [ ] **Step 5: Call prewarm after scene construction + renderer init/resize and before entering interactive visible loop.**
- [ ] **Step 6: Verify contract + strict compile.**
- [ ] **Step 7: Commit.**
```bash
git commit -m "Perf: prewarm imported renderer resources"
```

---

### Task 3: Static Directional Shadow Cache

**Files:**
- Create: `Sources/Renderer/Gpu/StaticShadowCacheGpu.hpp/.cpp`
- Create: `Sources/Renderer/Gpu/StaticShadowShader.hpp`
- Modify: direct-light files
- Test: `tests/static_shadow_cache_contract.cpp`

**Produces:**
```cpp
class StaticShadowCacheGpu {
public:
    static constexpr int SIZE=2048;
    bool init(std::string* error=nullptr);
    bool ensure(const Ecs::World&,const TriangleScene&,const RevisionState&,
                std::string* error=nullptr);
    bool validFor(const RevisionState&) const;
    GLuint depthTexture() const;
    const Math::Mat4& lightViewProjection() const;
    void shutdown();
};
```
- [ ] **Step 1: RED contract:** camera-only revision keeps cache valid; geometry/material/light revisions invalidate it.
- [ ] **Step 2: Implement orthographic fit to static world bounds with 5% padding, 2048x2048 depth, masked alpha cutoff, transparent/transmissive exclusion.**
- [ ] **Step 3: Add fixed 3x3 PCF lookup in direct-light consumer.**
- [ ] **Step 4: Directional static Sponza uses cache; dynamic lights/moving geometry retain exact BVH/ray fallback.**
- [ ] **Step 5: Run contract/build.**
- [ ] **Step 6: Commit.**
```bash
git commit -m "Perf: cache static directional shadows"
```

---

### Task 4: Reprojection History Cache

**Files:**
- Create: `Sources/Renderer/Gpu/ReprojectionPolicy.hpp`
- Create: `Sources/Renderer/Gpu/ReprojectionCacheGpu.hpp/.cpp`
- Create: `Sources/Renderer/Gpu/ReprojectionShader.hpp`
- Modify: render/Lumen integration
- Test: `tests/reprojection_cache_contract.cpp`

**Produces:**
```cpp
struct ReprojectionPolicy {
    static float positionTolerance(float camera_distance) {
        return std::max(0.03f,0.01f*camera_distance);
    }
    static constexpr float NORMAL_DOT_MIN=0.94f;
};
```
History stores previous world position/depth, normal, exact `R32UI` material ID, static direct, indirect, reflection.

- [ ] **Step 1: RED pure validation contract:** accept only when projected UV is inside `[0,1]`, previous depth is valid, material ID is exact, position error <= `max(0.03,0.01*camera_distance)`, and normal dot >= 0.94.
- [ ] **Step 2: Implement history textures/lifecycle and reprojection compute pass. No color similarity test.**
- [ ] **Step 3: Camera-only motion reprojects previous lighting without invalidating static shadows/world radiance.**
- [ ] **Step 4: Verify.**
- [ ] **Step 5: Commit.**
```bash
git commit -m "Perf: reproject static lighting history"
```

---

### Task 5: 8x8 Dirty Tile Compaction

**Files:**
- Create: `Sources/Renderer/Gpu/DirtyTileGpu.hpp/.cpp`
- Create: `Sources/Renderer/Gpu/DirtyTileShader.hpp`
- Test: `tests/dirty_tile_contract.cpp`

**Produces:**
```cpp
class DirtyTileGpu {
public:
    static constexpr int TILE_SIZE=8;
    bool resize(int width,int height,std::string* error=nullptr);
    bool compact(GLuint valid_mask,std::string* error=nullptr);
    GLuint tileBuffer() const;
    std::uint32_t totalCount() const;
};
```
- [ ] **Step 1: RED CPU policy test:** any invalid pixel dirties its whole 8x8 tile.
- [ ] **Step 2: Implement GPU dirty flags + atomic compacted tile-coordinate SSBO.**
- [ ] **Step 3: Expose compacted tiles to expensive direct/Lumen miss work; reusable tiles dispatch no expensive shading.**
- [ ] **Step 4: Verify.**
- [ ] **Step 5: Commit.**
```bash
git commit -m "Perf: compact dirty shading tiles"
```

---

### Task 6: Static Diffuse / View Specular Split

**Files:**
- Create: `Sources/Renderer/Gpu/StaticDiffuseLightingGpu.hpp/.cpp`
- Create: `Sources/Renderer/Gpu/ViewSpecularGpu.hpp/.cpp`
- Modify: direct-light/render orchestration
- Test: `tests/static_direct_split_contract.cpp`

**Produces:**
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
- [ ] **Step 1: RED invalidation test:** camera change preserves static diffuse but requires view specular.
- [ ] **Step 2: Move camera-independent diffuse, static directional shadow, emissive, ambient into static cache.**
- [ ] **Step 3: Keep Fresnel/specular/reflection weighting/transmission view response current; dynamic lights remain dynamic path.**
- [ ] **Step 4: Verify final direct term = static diffuse + current view-dependent term + dynamic-light term.**
- [ ] **Step 5: Commit.**
```bash
git commit -m "Perf: split cached diffuse and view specular"
```

---

### Task 7: Sparse World-Space Radiance Cache

**Files:**
- Create: `Sources/Renderer/Gpu/RadianceCacheGpu.hpp/.cpp`
- Create: `Sources/Renderer/Gpu/RadianceCacheShader.hpp`
- Modify: imported-Lumen path
- Test: `tests/radiance_cache_gpu_contract.cpp`

**Produces:**
```cpp
struct RadianceCachePolicy {
    static constexpr float CELL_SIZE=0.5f;
    static constexpr std::uint32_t INITIAL_CAPACITY=65536u;
    static constexpr std::uint32_t HIGH_CONFIDENCE_SAMPLES=16u;
    static constexpr std::uint32_t ACCEPT_CONFIDENCE=4u;
    static constexpr std::uint32_t MAX_LINEAR_PROBES=8u;
};
```
Record key = signed integer cell coordinate + radiance generation; value = RGB irradiance/radiance, confidence/sample count, accumulated dominant normal.

- [ ] **Step 1: RED generation/hash test:** camera revision does not advance generation; geometry/material/light does.
- [ ] **Step 2: Implement power-of-two 65536-slot table, max 8 linear probes; failed insert falls back to uncached trace without blocking.**
- [ ] **Step 3: Lookup eight neighboring cells, accept generation match + confidence >=4, blend trilinear spatial weights * normal agreement.**
- [ ] **Step 4: Miss/low confidence performs imported trace and updates nearest probe; stop increasing confidence after 16 samples.**
- [ ] **Step 5: Verify.**
- [ ] **Step 6: Commit.**
```bash
git commit -m "Perf: add static GPU radiance cache"
```

---

### Task 8: Reflection Fallback Cache

**Files:**
- Create: `Sources/Renderer/Gpu/ReflectionCacheGpu.hpp/.cpp`
- Modify: Lumen reflection path
- Test: `tests/reflection_cache_gpu_contract.cpp`

**Fixed fallback order:**
1. current screen-space hit;
2. world-space radiance cache for rough reflections;
3. validated previous reflection history for smooth surfaces;
4. imported triangle trace.

**Validation:** same position tolerance as `ReprojectionPolicy`, normal dot >=0.96, roughness difference <=0.05, exact material ID.

- [ ] **Step 1: RED fallback-order/validation contract.**
- [ ] **Step 2: Implement cache/history resources and pure source-selection helper.**
- [ ] **Step 3: Integrate into Lumen reflection path.**
- [ ] **Step 4: Verify.**
- [ ] **Step 5: Commit.**
```bash
git commit -m "Perf: cache reflection fallbacks"
```

---

### Task 9: Compact Imported Ray Geometry

**Files:**
- Create: `Sources/Renderer/Gpu/ShadowTriangleGpu.hpp`
- Create: `Sources/Renderer/Gpu/TraceGeometryGpu.hpp/.cpp`
- Modify: `TriangleScene` + imported shadow/Lumen traversal
- Test: `tests/trace_geometry_layout_contract.cpp`

**Produces:** separate visibility and GI/reflection buffers. Shadow records contain three positions + compact material/flags reference; they do not carry full 128-byte normal/material payload.

- [ ] **Step 1: RED `sizeof`/layout contract proving shadow record is smaller than current `TriangleGpu`.**
- [ ] **Step 2: Build compact buffers only when static geometry revision changes.**
- [ ] **Step 3: Opaque shadow traversal never fetches UV/material payload; masked/transmission candidates fetch only when needed.**
- [ ] **Step 4: GI/reflection retains UV/normals/material identity.**
- [ ] **Step 5: Run existing imported hit/shadow/Lumen semantic contracts.**
- [ ] **Step 6: Commit.**
```bash
git commit -m "Perf: compact imported ray geometry"
```

---

### Task 10: 16-Bin SAH + Front-to-Back Imported BVH

**Files:**
- Modify: `Sources/Renderer/Gpu/Bvh.hpp/.cpp`
- Modify: imported traversal shaders
- Test: `tests/bvh_sah_contract.cpp`
- Test: `tests/bvh_semantic_equivalence_contract.cpp`

**Produces:**
```cpp
BvhBuild buildBvhSah(const std::vector<BvhBoundsInput>&,
                     std::size_t leaf_size=3u,
                     std::size_t bin_count=16u);
```
- [ ] **Step 1: RED deterministic leaf-coverage/bounds contract.**
- [ ] **Step 2: RED ray equivalence contract vs brute-force/pre-optimization hits/distances.**
- [ ] **Step 3: Implement 16-bin SAH; use existing median/centroid split when SAH has empty side or no cost improvement.**
- [ ] **Step 4: GPU traversal computes child AABB entry distance and visits nearest first; opaque visibility early-exits; masked/transmission stays exact; TLAS refit preserved.**
- [ ] **Step 5: Verify semantic equivalence.**
- [ ] **Step 6: Commit.**
```bash
git commit -m "Perf: improve imported BVH traversal"
```

---

### Task 11: GBuffer Bandwidth Reduction — Stage A First

**Files:**
- Create: `Sources/Renderer/Gpu/GBufferReconstruct.hpp`
- Modify: `GBufferGpu`, `SurfaceFormats`, direct/Lumen/reprojection consumers
- Test: `tests/gbuffer_reconstruct_contract.cpp`
- Test: `tests/gbuffer_bandwidth_contract.cpp`

**Stage A requirements:** depth + inverse view-projection reconstruct world position; exact `R32UI` material ID; move material constants uniform across a material into material SSBO; keep texture-varying values per pixel only when required.

- [ ] **Step 1: RED world-position projection/reconstruction round-trip contract.**
- [ ] **Step 2: RED attachment-byte-count contract proving Stage A reduces per-pixel bandwidth.**
- [ ] **Step 3: Implement reconstruction and material-ID attachment; update every consumer before removing old position dependency.**
- [ ] **Step 4: Run full material/GBuffer contracts and RendererCheck visual smoke.**
- [ ] **Step 5: Evaluate Stage B octahedral normal packing only after Stage A visual verification; adopt only if no visual/material-normal regression, otherwise retain current normal precision.**
- [ ] **Step 6: Commit.**
```bash
git commit -m "Perf: reduce GBuffer bandwidth"
```

---

### Task 12: Explicit Static Scene Simulation Policy

**Files:**
- Modify: `Sources/Renderer/Test/TestScene.hpp/.cpp`
- Modify: `Sources/main.cpp`
- Test: `tests/static_scene_simulation_contract.cpp`

**Produces:**
```cpp
struct SceneState {
    // existing fields
    bool dynamic=true;
};
```
- [ ] **Step 1: RED test:** normal interactive Sponza `dynamic=false`; moving-object/light RendererCheck scenes `dynamic=true`.
- [ ] **Step 2: Set dynamics explicitly during scene construction; never infer from history/entity count.**
- [ ] **Step 3: Guard the fixed simulation loop with `scene_state.dynamic`; camera update remains independent.**
```cpp
if(!renderercheck_mode && !performance_static_scene && scene_state.dynamic) {
    // existing fixed-step Test::updateScene loop
}
```
- [ ] **Step 4: Verify.**
- [ ] **Step 5: Commit.**
```bash
git commit -m "Perf: skip static scene simulation"
```

---

### Task 13: Low-FPS GPU Profiling + Cache Statistics

**Files:**
- Create: `Sources/Renderer/Gpu/CacheStats.hpp`
- Modify: `Sources/Renderer/Gpu/Profiler.hpp/.cpp`
- Wire cache counters under profiling flag
- Test: `tests/low_fps_profiler_contract.cpp`

**Produces:**
```cpp
struct CacheStats {
    std::uint32_t dirty_tiles=0,total_tiles=0;
    std::uint64_t reused_pixels=0,total_pixels=0;
    std::uint64_t radiance_hits=0,radiance_queries=0;
    std::uint64_t reflection_hits=0,reflection_queries=0;
    bool static_shadow_cached=false;
};
```
When `CRAPGAME_GPU_PROFILE=1`: issue timer queries every rendered frame; aggregate asynchronously; never block; print at most once/sec when results exist. Required passes/counters: geometry, static shadow generation/cached, static diffuse/direct, view specular, reprojection, dirty tiles, reused pixels, Lumen trace, radiance hit %, reflection hit %, composite, present.

- [ ] **Step 1: RED cadence/formatting contract.**
- [ ] **Step 2: Extend profiler pass enum and cache-stat handoff without changing normal performance-writer semantics.**
- [ ] **Step 3: Ensure default gameplay without profiling flag performs no CPU query-result/readback formatting work.**
- [ ] **Step 4: Verify.**
- [ ] **Step 5: Commit.**
```bash
git commit -m "Perf: expose low-FPS GPU profiling"
```

---

### Task 14: End-to-End Frame Scheduling + Final Verification

**Files:**
- Modify: `Sources/Renderer/Render.hpp/.cpp`
- Test: `tests/static_sponza_frame_policy_contract.cpp`
- Modify CI contract workflow only if necessary for focused contract inclusion; do not broaden triggers.

**Produces:**
```cpp
struct FrameWork {
    bool geometry=false, shadow=false, reprojection=false, dirty_tiles=false;
    bool static_diffuse=false, view_specular=false, lumen_trace=false;
    bool composite=false, transparent=false, present=true;
};
FrameWork decideFrameWork(const RevisionState& previous,
                          const RevisionState& current,
                          bool converged,
                          bool camera_moving,
                          bool transparent_dynamic);
```

**Scheduling rules:**
- scene/light/material invalidation -> immediate affected work;
- camera change -> reprojection + bounded refresh; static shadow/world-radiance remain valid;
- continuous camera motion -> expensive secondary refresh at most every `66,666,667 ns` (15 Hz), intermediate frames use caches/reprojection;
- unchanged camera/scene -> converge to default 8 then freeze;
- frozen state -> no geometry/direct/shadow/Lumen/composite solely because time passed;
- `CRAPGAME_LUMEN_HZ` may alter moving-camera interval but not revision invalidation/static freeze unless explicit force flag is set.

- [ ] **Step 1: RED stationary policy test:** after convergence only presentation remains for static opaque Sponza.
```cpp
FrameWork w=decideFrameWork(r,r,true,false,false);
require(!w.geometry&&!w.shadow&&!w.static_diffuse&&!w.view_specular);
require(!w.lumen_trace&&!w.composite&&w.present);
```
- [ ] **Step 2: RED camera-motion test:** native geometry + reprojection/dirty/view work allowed; static shadow/radiance generation not invalidated.
- [ ] **Step 3: Centralize orchestration in `decideFrameWork`; keep implementation details in subsystem classes.**
- [ ] **Step 4: Run all focused strict contracts.**
- [ ] **Step 5: Run full build.**
```bash
c build
```
- [ ] **Step 6: Run interactive Sponza with profiling on available GL4.3 hardware.**
```bash
CRAPGAME_GPU_PROFILE=1 c build run
```
Record actual timings/cache hit rates; do not invent FPS if comparable hardware is unavailable.
- [ ] **Step 7: Run RendererCheck smoke/contracts; crashes, validation errors, timeouts, or non-baseline failures are failures.**
- [ ] **Step 8: Audit.**
```bash
git diff --check
git status --short
grep -R -n -E 'stb_image|tinyobj|assimp' Sources || true
```
- [ ] **Step 9: Commit final orchestration/verification changes.**
```bash
git commit -m "Perf: finish static scene caching path"
```
