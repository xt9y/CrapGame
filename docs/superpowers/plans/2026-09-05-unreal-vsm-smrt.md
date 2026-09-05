# Unreal VSM + SMRT Parity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace CrapGame's monolithic 2048x2048 PCF/static-shadow path and binary direct-shadow fallback with UE-style virtual shadow maps, directional clipmaps, receiver-driven sparse pages, persistent page caching, and SMRT finite-source soft-shadow projection while preserving the existing renderer frontend and source style.

**Architecture:** `Renderer::Rendering` and `Gpu::DirectLightingGpu` remain the orchestration boundaries. New focused GPU classes under `Sources/Renderer/Gpu/` own the virtual address space, physical-page cache, clipmap transforms, page marking/raster work, and SMRT projection; `DirectLightingGpu` consumes their visibility result exactly where the current `StaticShadowCacheGpu` result is used. The old static-shadow classes remain only until the new path is integrated and verified, then are removed from the active renderer.

**Tech Stack:** C++17, OpenGL 4.3-era functionality exposed through `lwcgl` `v2.9.3`, GLSL 4.30 compute/graphics shaders, SSBOs, FBOs, texture atlases, existing ECS, existing BVH/triangle scene, custom C-BuildSystem, RendererCheck.

**Spec:** `docs/superpowers/specs/2026-09-05-unreal-engine-renderer-parity-design.md`

## Global Constraints

- `Renderer::Rendering` remains the only public renderer frontend.
- Preserve existing CrapGame naming, file layout, class shape, include ordering, braces, wrapping, camelCase functions, snake_case variables, PascalCase files/directories, and current minimal public methods.
- Only `xt9y/lwcgl` branch `v2.9.3` is permitted for graphics/window/input access.
- Before adding any missing `lwcgl` binding, verify the function/class/constant existed in original LWJGL 2.9.3; add only the generic source-shaped/native-equivalent binding and compatibility coverage in `lwcgl`.
- Do not introduce Vulkan, DX12, DXR, Metal, CUDA, OptiX, newer LWJGL APIs, or CrapGame-specific APIs inside `lwcgl`.
- No CPU readback or blocking GPU synchronization is added to the normal shadow hot path.
- Visible direct shadows, Lumen Surface Cache shadowing, GI occlusion, and short-range AO stay separate signals.
- Default UE-reference values begin at: 128x128 physical page size, 128x128 level-0 pages, 16384x16384 nominal virtual resolution, 2048 physical pages, 8x8 receiver mask, clipmap levels 6..22, coarse levels 15..18, Z range scale 1000, page max age 1000, pool-pressure threshold 0.85, dynamic LOD bias max 2.0, normal bias 0.5, screen-ray length 0.015, directional/local SMRT 7 rays x 8 samples, directional ray-length scale 1.5, directional extrapolate slope 5.0, local extrapolate slope 0.05.
- Build system already recursively compiles `Sources/*/*/*.cpp`; new `Sources/Renderer/Gpu/*.cpp` files require no build-system source-list edits.
- Work is performed on `main` as previously approved.

---

## File Map

**Create:**
- `Sources/Renderer/Gpu/VirtualShadowPolicy.hpp`
- `Sources/Renderer/Gpu/ShadowPageCachePolicy.hpp`
- `Sources/Renderer/Gpu/ShadowPageCacheGpu.hpp`
- `Sources/Renderer/Gpu/ShadowPageCacheGpu.cpp`
- `Sources/Renderer/Gpu/VirtualShadowMapGpu.hpp`
- `Sources/Renderer/Gpu/VirtualShadowMapGpu.cpp`
- `Sources/Renderer/Gpu/VirtualShadowMapShader.hpp`
- `Sources/Renderer/Gpu/SmrtShadowPolicy.hpp`
- `Sources/Renderer/Gpu/SmrtShadowShader.hpp`
- `tests/virtual_shadow_policy_contract.cpp`
- `tests/shadow_page_cache_contract.cpp`
- `tests/virtual_shadow_shader_contract.cpp`
- `tests/smrt_shadow_contract.cpp`
- `tests/virtual_shadow_integration_contract.cpp`

**Modify:**
- `Sources/Ecs/Ecs.hpp`
- `Sources/Renderer/Gpu/DirectLightingGpu.hpp`
- `Sources/Renderer/Gpu/DirectLightingGpu.cpp`
- `Sources/Renderer/Gpu/DirectLightingScene.cpp`
- `Sources/Renderer/Gpu/DirectLightingImported.cpp`
- `Sources/Renderer/Gpu/DirectLightingImportedShader.hpp`
- `Sources/Renderer/Gpu/DirectLightingShader.hpp`
- `Sources/Renderer/Gpu/StaticDiffuseLightingGpu.hpp/.cpp`
- `Sources/Renderer/Gpu/ViewSpecularGpu.hpp/.cpp`
- `Sources/Renderer/Gpu/ScenePrewarm.hpp`
- `Sources/Renderer/Gpu/FrameWorkPolicy.hpp`
- `Sources/Renderer/Gpu/CacheStats.hpp`
- `Sources/Renderer/Gpu/Profiler.hpp/.cpp`
- `Sources/Renderer/Render.cpp`
- `Sources/Renderer/Test/TestScene.cpp`
- `Sources/Renderer/Debug.cpp`
- `rendercheck.toml`

**Remove after replacement is verified:**
- `Sources/Renderer/Gpu/StaticShadowCacheGpu.hpp`
- `Sources/Renderer/Gpu/StaticShadowCacheGpu.cpp`
- `Sources/Renderer/Gpu/StaticShadowPolicy.hpp`
- `Sources/Renderer/Gpu/StaticShadowShader.hpp`
- replace/update `tests/static_shadow_cache_contract.cpp` so it no longer asserts the obsolete PCF path.

---

### Task 1: Virtual Shadow Policy + Finite-Light Metadata

**Files:**
- Create: `Sources/Renderer/Gpu/VirtualShadowPolicy.hpp`
- Modify: `Sources/Ecs/Ecs.hpp`
- Modify: `Sources/Renderer/Gpu/DirectLightingGpu.hpp`
- Modify: `Sources/Renderer/Gpu/DirectLightingScene.cpp`
- Test: `tests/virtual_shadow_policy_contract.cpp`

**Interfaces:**
- Consumes: existing `Ecs::LightComponent`, `DirectLightingGpu::LightGpu` upload path.
- Produces:

```cpp
struct VirtualShadowPolicy
{
    static constexpr int PAGE_SIZE = 128;
    static constexpr int LEVEL0_PAGES = 128;
    static constexpr int VIRTUAL_RESOLUTION = PAGE_SIZE * LEVEL0_PAGES;
    static constexpr int MAX_MIP_LEVELS = 8;
    static constexpr int MAX_PHYSICAL_PAGES = 2048;
    static constexpr int RECEIVER_MASK_SIZE = 8;
    static constexpr int FIRST_CLIPMAP_LEVEL = 6;
    static constexpr int LAST_CLIPMAP_LEVEL = 22;
    static constexpr int FIRST_COARSE_LEVEL = 15;
    static constexpr int LAST_COARSE_LEVEL = 18;
    static constexpr int MAX_PAGE_AGE = 1000;
    static constexpr float Z_RANGE_SCALE = 1000.0f;
    static constexpr float PAGE_PRESSURE_THRESHOLD = 0.85f;
    static constexpr float MAX_DYNAMIC_LOD_BIAS = 2.0f;
    static constexpr float NORMAL_BIAS = 0.5f;
    static constexpr float SCREEN_RAY_LENGTH = 0.015f;
};

inline int virtualShadowMipLevel (float footprint);
inline float virtualShadowClipmapExtent (int level);
inline float virtualShadowDynamicLodBias (int requested_pages);
```

Append to `Ecs::LightComponent` with defaults so existing aggregate initializers remain source-compatible:

```cpp
float source_radius = 0.0f,
      source_angle = 0.0f;
```

Expand `DirectLightingGpu::LightGpu` from four vec4 records to five by adding:

```cpp
float source_shape[4];
```

with `source_shape[0] = light->source_radius`, `source_shape[1] = Math::radians(light->source_angle)`, and remaining values zero for this stage.

- [ ] **Step 1: Write the failing policy contract.**

```cpp
#include "Renderer/Gpu/VirtualShadowPolicy.hpp"
#include <cstdlib>
#include <iostream>

static void require(bool value,const char *message)
{
    if(!value){std::cerr<<message<<'\n';std::exit(1);}
}

int main()
{
    using namespace Renderer::Gpu;
    require(VirtualShadowPolicy::PAGE_SIZE==128,"VSM page size");
    require(VirtualShadowPolicy::LEVEL0_PAGES==128,"VSM level-0 page count");
    require(VirtualShadowPolicy::VIRTUAL_RESOLUTION==16384,"VSM virtual resolution");
    require(VirtualShadowPolicy::MAX_PHYSICAL_PAGES==2048,"VSM physical-page budget");
    require(VirtualShadowPolicy::RECEIVER_MASK_SIZE==8,"VSM receiver mask");
    require(VirtualShadowPolicy::FIRST_CLIPMAP_LEVEL==6&&VirtualShadowPolicy::LAST_CLIPMAP_LEVEL==22,"directional clipmap range");
    require(VirtualShadowPolicy::FIRST_COARSE_LEVEL==15&&VirtualShadowPolicy::LAST_COARSE_LEVEL==18,"coarse clipmap range");
    require(virtualShadowMipLevel(1.0f)==0,"unit footprint selects mip zero");
    require(virtualShadowMipLevel(8.0f)==3,"eight-texel footprint selects mip three");
    require(virtualShadowDynamicLodBias(1740)>0.0f,"pool pressure raises LOD bias");
    require(virtualShadowDynamicLodBias(1000)==0.0f,"healthy pool keeps full resolution");
    std::cout<<"virtual_shadow_policy_contract=PASS\n";
}
```

- [ ] **Step 2: Run RED.**

```bash
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -ISources tests/virtual_shadow_policy_contract.cpp -o /tmp/virtual-shadow-policy
```

Expected: fail because `VirtualShadowPolicy.hpp` does not exist.

- [ ] **Step 3: Implement the policy helpers.** Use `floor(log2(max(footprint,1)))` clamped to `[0, MAX_MIP_LEVELS-1]`; clipmap extent is `exp2(level)`; pressure starts at `MAX_PHYSICAL_PAGES * PAGE_PRESSURE_THRESHOLD` and linearly reaches `MAX_DYNAMIC_LOD_BIAS` at full pool.

- [ ] **Step 4: Add source extent to ECS/GPU light records without changing public renderer APIs.** Directional test/default Sponza light gets `source_angle = 0.5357f`; point/spot test lights use a small non-zero `source_radius` only in dedicated soft-shadow tests added later in this plan.

- [ ] **Step 5: Run contract plus strict compilation of the touched scene uploader.**

```bash
/tmp/virtual-shadow-policy
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -ISources -c Sources/Renderer/Gpu/DirectLightingScene.cpp -o /tmp/direct-lighting-scene.o
```

- [ ] **Step 6: Commit.**

```bash
git add Sources/Ecs/Ecs.hpp Sources/Renderer/Gpu/VirtualShadowPolicy.hpp Sources/Renderer/Gpu/DirectLightingGpu.hpp Sources/Renderer/Gpu/DirectLightingScene.cpp tests/virtual_shadow_policy_contract.cpp
git commit -m "Renderer: add virtual shadow policy"
```

---

### Task 2: Persistent Physical-Page Cache

**Files:**
- Create: `Sources/Renderer/Gpu/ShadowPageCachePolicy.hpp`
- Create: `Sources/Renderer/Gpu/ShadowPageCacheGpu.hpp`
- Create: `Sources/Renderer/Gpu/ShadowPageCacheGpu.cpp`
- Modify: `Sources/Renderer/Gpu/CacheStats.hpp`
- Test: `tests/shadow_page_cache_contract.cpp`

**Interfaces:**

```cpp
struct ShadowPageKey
{
    std::uint32_t light = 0u;
    std::uint16_t level = 0u;
    std::uint16_t mip = 0u;
    std::int32_t x = 0;
    std::int32_t y = 0;
};

struct ShadowPageState
{
    ShadowPageKey key = {};
    std::uint32_t physical = 0u;
    std::uint64_t last_requested = 0u;
    std::uint64_t revision = 0u;
    bool allocated = false;
    bool dirty = false;
    bool dynamic = false;
};

inline bool sameShadowPageKey(const ShadowPageKey&,const ShadowPageKey&);
inline int chooseShadowPageEviction(const std::vector<ShadowPageState>&,std::uint64_t frame_index);

class ShadowPageCacheGpu
{
public:
    bool init (std::string *error = nullptr);
    void beginFrame (std::uint64_t frame_index);
    bool ensurePage (const ShadowPageKey& key,bool dynamic,std::uint64_t revision,
                     std::uint32_t *physical,std::string *error = nullptr);
    void invalidateRevision (std::uint64_t revision);
    void invalidateLight (std::uint32_t light);
    void endFrame ();
    void shutdown ();
    GLuint metadataBuffer () const;
    GLuint pageTableBuffer () const;
    std::uint32_t requestedThisFrame () const;
    std::uint32_t renderedThisFrame () const;
    std::uint32_t cachedThisFrame () const;
};
```

`CacheStats` gains:

```cpp
std::uint64_t shadow_pages_requested=0u;
std::uint64_t shadow_pages_rendered=0u;
std::uint64_t shadow_pages_cached=0u;
std::uint64_t shadow_pages_evicted=0u;
std::uint64_t shadow_static_invalidated=0u;
std::uint64_t shadow_dynamic_invalidated=0u;
```

- [ ] **Step 1: Write RED cache-policy contract** covering exact-key reuse, oldest-unrequested eviction, no eviction of a page requested this frame, and max-age preference.

```cpp
std::vector<ShadowPageState> pages(3);
pages[0].allocated=true;pages[0].last_requested=10;
pages[1].allocated=true;pages[1].last_requested=40;
pages[2].allocated=true;pages[2].last_requested=100;
require(chooseShadowPageEviction(pages,100)==0,"oldest page evicts first");
pages[0].last_requested=100;
require(chooseShadowPageEviction(pages,100)==1,"current-frame request is protected");
```

- [ ] **Step 2: Run RED.**

```bash
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -ISources tests/shadow_page_cache_contract.cpp -o /tmp/shadow-page-cache
```

- [ ] **Step 3: Implement CPU-side cache metadata only for scheduling/bookkeeping.** Allocate metadata vectors once to `MAX_PHYSICAL_PAGES`; reuse slots; never allocate per frame. `ensurePage()` returns an existing physical page when key/revision is valid, otherwise picks a free slot then the oldest non-requested page.

- [ ] **Step 4: Allocate generic GL buffers in `ShadowPageCacheGpu::init()`.** Use only existing LWJGL-2.9.3-era `GL15.glGenBuffers`, `GL15.glBufferData`, `GL15.glBufferSubData`, `GL30.glBindBufferBase`; no new lwcgl API is required for this task.

- [ ] **Step 5: Update `cacheStatsDelta()` for all new counters and extend `tests/cache_stats_contract.cpp` to assert one shadow-page delta.**

- [ ] **Step 6: Run contracts/build.**

```bash
/tmp/shadow-page-cache
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -ISources tests/cache_stats_contract.cpp -o /tmp/cache-stats && /tmp/cache-stats
c build
```

- [ ] **Step 7: Commit.**

```bash
git add Sources/Renderer/Gpu/ShadowPageCachePolicy.hpp Sources/Renderer/Gpu/ShadowPageCacheGpu.* Sources/Renderer/Gpu/CacheStats.hpp tests/shadow_page_cache_contract.cpp tests/cache_stats_contract.cpp
git commit -m "Renderer: add persistent shadow page cache"
```

---

### Task 3: Directional Clipmap Math + Stable Page Translation

**Files:**
- Create: `Sources/Renderer/Gpu/VirtualShadowMapGpu.hpp`
- Create: `Sources/Renderer/Gpu/VirtualShadowMapGpu.cpp`
- Test: extend `tests/virtual_shadow_policy_contract.cpp`

**Interfaces:**

```cpp
struct VirtualShadowClipmap
{
    Math::Vec3 origin = {0.0f,0.0f,0.0f};
    Math::Mat4 view_projection = Math::identity();
    float extent = 0.0f;
    float texel_world_size = 0.0f;
    int level = 0;
    int page_offset_x = 0;
    int page_offset_y = 0;
};

class VirtualShadowMapGpu
{
public:
    bool init (std::string *error = nullptr);
    bool resize (int width,int height,std::string *error = nullptr);
    bool update (
                const Ecs::World& world,
                const GBufferGpu& gbuffer,
                const TriangleScene& triangles,
                const Math::Vec3& camera_position,
                std::uint64_t frame_index,
                std::uint64_t scene_revision,
                std::string *error = nullptr
        );
    bool bind (GLuint program,std::string *error = nullptr) const;
    void shutdown ();
    bool ready () const;
    const ShadowPageCacheGpu& pageCache () const;
};

VirtualShadowClipmap directionalShadowClipmap(
        int level,const Math::Vec3& camera_position,const Math::Vec3& light_direction);
```

- [ ] **Step 1: Add RED assertions** that level `n+1` has exactly twice level `n` world extent and that camera movement smaller than one page does not change snapped origin/page offset.

```cpp
const VirtualShadowClipmap a=directionalShadowClipmap(6,{0,0,0},{0,-1,0});
const VirtualShadowClipmap b=directionalShadowClipmap(7,{0,0,0},{0,-1,0});
require(std::fabs(b.extent-a.extent*2.0f)<0.001f,"clipmap coverage doubles");
const VirtualShadowClipmap c=directionalShadowClipmap(6,{a.texel_world_size*32.0f,0,0},{0,-1,0});
require(c.page_offset_x==a.page_offset_x,"sub-page movement keeps cached page translation");
```

- [ ] **Step 2: Implement directional view basis and page-snapped origins.** Snap in light-space by `PAGE_SIZE * texel_world_size`, not by individual texel, so whole physical pages translate/reuse.

- [ ] **Step 3: Build all levels `6..22` each update, but do not allocate physical pages yet.** Store fixed-size clipmap state; no per-frame vector allocation.

- [ ] **Step 4: Add `VirtualShadowMapGpu` resource lifetime skeleton using the same `init/resize/update/bind/shutdown/ready` style as existing GPU classes.** Own a `ShadowPageCacheGpu page_cache_` member.

- [ ] **Step 5: Run policy contract and strict object compile.**

```bash
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -ISources tests/virtual_shadow_policy_contract.cpp Sources/Renderer/Gpu/VirtualShadowMapGpu.cpp Sources/Renderer/Gpu/ShadowPageCacheGpu.cpp -lGL -llwcgl -o /tmp/virtual-shadow-policy
```

- [ ] **Step 6: Commit.**

```bash
git add Sources/Renderer/Gpu/VirtualShadowMapGpu.* tests/virtual_shadow_policy_contract.cpp
git commit -m "Renderer: add directional shadow clipmaps"
```

---

### Task 4: Receiver-Driven Page Marking + Coarse Pages

**Files:**
- Create: `Sources/Renderer/Gpu/VirtualShadowMapShader.hpp`
- Modify: `Sources/Renderer/Gpu/VirtualShadowMapGpu.hpp/.cpp`
- Test: `tests/virtual_shadow_shader_contract.cpp`

**Interfaces:**
- `VirtualShadowMapGpu::update()` reconstructs visible receiver world positions from the existing GBuffer depth texture.
- 8x8 compute tiles mark virtual pages into an SSBO bitset/record buffer.
- Per receiver: select directional clipmap level from screen footprint, derive virtual UV/page, mark the detailed page, and additionally mark coarse pages for levels 15..18.
- Requested records are compacted into a fixed-capacity GPU work buffer; allocation/overflow counters are separate from physical-page cache counters.

Required shader declarations:

```glsl
layout(local_size_x=8,local_size_y=8,local_size_z=1) in;
layout(binding=0) uniform sampler2D sDepth;
layout(std430,binding=9) buffer ShadowPageRequests { uint shadowPageRequests[]; };
layout(std430,binding=10) buffer ShadowRequestCount { uint shadowRequestCount; };
uniform mat4 uGBufferInverseViewProjection;
uniform int uShadowClipmapCount;
```

- [ ] **Step 1: Write RED shader-source contract** requiring `local_size_x=8`, depth reconstruction, receiver-mask quantization by 8, page-size 128 math, coarse-level marking, atomic request compaction, and no loop over all world shadow pages.

- [ ] **Step 2: Run RED.**

```bash
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -ISources tests/virtual_shadow_shader_contract.cpp -o /tmp/virtual-shadow-shader
```

- [ ] **Step 3: Implement receiver marking shader.** Reuse the existing `GBUFFER_RECONSTRUCT_GLSL` helper rather than creating a duplicate world-position path.

- [ ] **Step 4: Implement fixed-capacity request storage.** Clear only the request counter/bitset needed for the current frame; do not recreate buffers. Use `GL43.glDispatchCompute` and `GL42.glMemoryBarrier` already exposed by lwcgl.

- [ ] **Step 5: Implement dynamic resolution pressure.** Apply `virtualShadowDynamicLodBias()` when requested-page count from the previous frame/persistent counter is above 85% capacity; never synchronously read a fresh GPU counter to CPU in the frame hot path.

- [ ] **Step 6: Run contract/build.**

```bash
/tmp/virtual-shadow-shader
c build
```

- [ ] **Step 7: Commit.**

```bash
git add Sources/Renderer/Gpu/VirtualShadowMapShader.hpp Sources/Renderer/Gpu/VirtualShadowMapGpu.* tests/virtual_shadow_shader_contract.cpp
git commit -m "Renderer: mark virtual shadow receiver pages"
```

---

### Task 5: Physical Page Atlas + Page-Aware Caster Submission

**Files:**
- Modify: `Sources/Renderer/Gpu/VirtualShadowMapGpu.hpp/.cpp`
- Modify: `Sources/Renderer/Gpu/VirtualShadowMapShader.hpp`
- Reuse: `Renderer/Gpu/TriangleScene.hpp/.cpp`, existing BVH/TLAS buffers, `MaterialGpu` masked-alpha path.
- Test: extend `tests/virtual_shadow_shader_contract.cpp`

**Interfaces:**
- Physical pages live in one persistent depth atlas arranged as 64x32 pages: `8192 x 4096` texels for 2048 x 128x128 pages.
- `physicalAtlasUv(physical_page, page_uv)` maps a physical index to atlas UV.
- Each dirty/requested page gets a light-space page frustum and a caster list generated from existing instance bounds/TLAS before raster.
- Masked materials preserve opacity-texture alpha cutoff; transparent/transmissive materials do not become opaque shadow casters.

- [ ] **Step 1: Add RED contract assertions** for atlas dimensions, page-to-atlas mapping, masked alpha support, BVH/TLAS page culling string/path, and absence of the old full-scene `SIZE=2048` orthographic render loop in the active class.

- [ ] **Step 2: Allocate one persistent `GL_DEPTH_COMPONENT24` atlas texture and FBO.** Set depth sampling to `GL_NEAREST`; manual SMRT comparisons must not rely on hardware filtering.

- [ ] **Step 3: Render only dirty physical pages.** Set viewport/scissor to the page's 128x128 atlas rectangle and submit casters intersecting that page frustum. Reuse mesh VAOs/material bindings already owned by renderer classes rather than duplicating model-loader ownership.

- [ ] **Step 4: Implement page-table fallback.** Missing high-resolution pages fall back to an available parent/coarse page; invalid page with no parent returns visible instead of sampling uninitialized depth.

- [ ] **Step 5: Implement cache invalidation semantics.** Geometry/material registry changes invalidate intersecting pages; light transform/property changes invalidate that light's pages; camera movement changes clipmap page translation but does not globally invalidate physical pages.

- [ ] **Step 6: Run strict compile and build.**

```bash
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -ISources -c Sources/Renderer/Gpu/VirtualShadowMapGpu.cpp -o /tmp/virtual-shadow-map.o
c build
```

- [ ] **Step 7: Commit.**

```bash
git add Sources/Renderer/Gpu/VirtualShadowMapGpu.* Sources/Renderer/Gpu/VirtualShadowMapShader.hpp tests/virtual_shadow_shader_contract.cpp
git commit -m "Renderer: raster virtual shadow pages"
```

---

### Task 6: SMRT Finite-Source Projection

**Files:**
- Create: `Sources/Renderer/Gpu/SmrtShadowPolicy.hpp`
- Create: `Sources/Renderer/Gpu/SmrtShadowShader.hpp`
- Modify: `Sources/Renderer/Gpu/VirtualShadowMapShader.hpp`
- Test: `tests/smrt_shadow_contract.cpp`

**Interfaces:**

```cpp
struct SmrtShadowPolicy
{
    static constexpr int DIRECTIONAL_RAYS = 7;
    static constexpr int DIRECTIONAL_SAMPLES_PER_RAY = 8;
    static constexpr int LOCAL_RAYS = 7;
    static constexpr int LOCAL_SAMPLES_PER_RAY = 8;
    static constexpr float DIRECTIONAL_RAY_LENGTH_SCALE = 1.5f;
    static constexpr float DIRECTIONAL_EXTRAPOLATE_MAX_SLOPE = 5.0f;
    static constexpr float LOCAL_EXTRAPOLATE_MAX_SLOPE = 0.05f;
    static constexpr bool ADAPTIVE_RAY_COUNT = true;
};
```

Shader entry point consumed by direct/static-diffuse/view-specular shaders:

```glsl
float virtualShadowVisibility(
    vec3 position,
    vec3 normal,
    vec3 lightDirection,
    int lightIndex,
    int lightType,
    float maximumDistance,
    float sourceRadius,
    float sourceAngle,
    uint frameIndex);
```

- [ ] **Step 1: Write RED SMRT contract** requiring 7/8 reference counts, adaptive path, frame-index jitter, finite source disk sampling, multi-sample ray march through virtual shadow depth, blocker distance accumulation, directional/local slope limits, `SCREEN_RAY_LENGTH`, `NORMAL_BIAS`, and no fixed-radius PCF loop.

```cpp
const std::string shader=SMRT_SHADOW_GLSL;
require(shader.find("DIRECTIONAL_RAYS")!=std::string::npos,"directional ray loop missing");
require(shader.find("frameIndex")!=std::string::npos,"temporal shadow jitter missing");
require(shader.find("blockerDistance")!=std::string::npos,"SMRT blocker distance missing");
require(shader.find("for (int y = -1") == std::string::npos,"fixed PCF must not remain in SMRT");
```

- [ ] **Step 2: Implement deterministic low-discrepancy disk sequence + per-pixel/frame rotation.** Keep it self-contained GLSL; do not introduce texture-noise assets unless already present.

- [ ] **Step 3: Implement smart receiver start.** Offset by normal scaled to virtual texel world footprint, derive slope-aware depth offset, then perform the short screen-space start adjustment using depth reconstruction over `SCREEN_RAY_LENGTH` before VSM sampling.

- [ ] **Step 4: Implement 7 source rays x up to 8 VSM depth samples.** Directional rays perturb around finite angular disk; local rays perturb toward finite source radius. Accumulate visible-ray fraction and blocker distance.

- [ ] **Step 5: Implement adaptive early-outs.** After the first ray/sample evidence, stop work for uniformly lit/umbra compute regions when GLSL subgroup/wave operations are available through a verified LWJGL 2.9.3 binding; otherwise preserve identical per-pixel early-out semantics without inventing an lwcgl API. If a subgroup extension binding is needed, verify it existed in LWJGL 2.9.3 before touching lwcgl and add a separate lwcgl compatibility test first.

- [ ] **Step 6: Implement bounded blocker-depth extrapolation and texel dithering.** Use 5.0 directional and 0.05 local max slope controls; keep this separate from receiver normal bias.

- [ ] **Step 7: Run contract/build.**

```bash
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -ISources tests/smrt_shadow_contract.cpp -o /tmp/smrt-shadow && /tmp/smrt-shadow
c build
```

- [ ] **Step 8: Commit.**

```bash
git add Sources/Renderer/Gpu/SmrtShadowPolicy.hpp Sources/Renderer/Gpu/SmrtShadowShader.hpp Sources/Renderer/Gpu/VirtualShadowMapShader.hpp tests/smrt_shadow_contract.cpp
git commit -m "Renderer: add SMRT soft shadow projection"
```

---

### Task 7: Direct-Lighting Integration + Remove Active PCF Path

**Files:**
- Modify: `Sources/Renderer/Gpu/DirectLightingGpu.hpp/.cpp`
- Modify: `Sources/Renderer/Gpu/DirectLightingScene.cpp`
- Modify: `Sources/Renderer/Gpu/DirectLightingImported.cpp`
- Modify: `Sources/Renderer/Gpu/DirectLightingImportedShader.hpp`
- Modify: `Sources/Renderer/Gpu/DirectLightingShader.hpp`
- Modify: `Sources/Renderer/Gpu/StaticDiffuseLightingGpu.hpp/.cpp`
- Modify: `Sources/Renderer/Gpu/ViewSpecularGpu.hpp/.cpp`
- Modify: `Sources/Renderer/Gpu/ScenePrewarm.hpp`
- Modify: `Sources/Renderer/Render.cpp`
- Test: `tests/virtual_shadow_integration_contract.cpp`

**Interfaces:**
- `DirectLightingGpu` replaces `StaticShadowCacheGpu static_shadow_cache_` with `VirtualShadowMapGpu virtual_shadow_map_`.
- `dispatch()` receives frame index:

```cpp
bool dispatch(
        const GBufferGpu& gbuffer,
        const Math::Vec3& camera_position,
        std::uint64_t frame_index,
        std::string *error = nullptr);
```

- `render()` forwards frame index or keeps a source-compatible overload that passes zero only for non-runtime tests.
- `StaticDiffuseLightingGpu::updateIfNeeded()` and `ViewSpecularGpu::render()` consume `const VirtualShadowMapGpu&` and call the same `virtualShadowVisibility()` shader logic as dynamic direct lighting.

- [ ] **Step 1: Write RED integration contract** that scans headers/shader assembly for `VirtualShadowMapGpu`, `virtualShadowVisibility`, frame index, source radius/angle, and absence of `StaticShadowCacheGpu`/`staticShadowVisibility` in the active direct-light classes.

- [ ] **Step 2: Initialize/resize/prewarm/shutdown `virtual_shadow_map_` inside `DirectLightingGpu` with the same ownership pattern as existing members.**

- [ ] **Step 3: Call `virtual_shadow_map_.update()` before direct/static-diffuse/view-specular shading whenever frame policy schedules shadow work or camera movement requires new receiver pages.** This is important: camera movement does not invalidate cached pages, but it can request new ones.

- [ ] **Step 4: Bind page atlas/page tables/clipmap metadata once per direct pass.** Replace all calls to `staticShadowVisibility()` with `virtualShadowVisibility()` for shadow-casting lights.

- [ ] **Step 5: Remove the old `static_split_light_index` concept from the final shadow-visibility path.** Static diffuse/view specular may stay split for performance, but their visibility comes from VSM/SMRT, not the old one-light monolithic cache.

- [ ] **Step 6: Pass `gpu_frame_index_` from `Rendering::renderGpuFrame()` to `DirectLightingGpu::dispatch()` so temporal SMRT jitter changes each rendered frame.

- [ ] **Step 7: Update frame policy.** `work.shadow` continues to mean world-shadow invalidation; camera-only frames must also permit receiver-page marking without declaring all cached VSM contents dirty.

- [ ] **Step 8: Run contract/build.**

```bash
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -ISources tests/virtual_shadow_integration_contract.cpp -o /tmp/virtual-shadow-integration && /tmp/virtual-shadow-integration
c build
```

- [ ] **Step 9: Commit.**

```bash
git add Sources/Renderer/Gpu/DirectLighting* Sources/Renderer/Gpu/StaticDiffuseLightingGpu.* Sources/Renderer/Gpu/ViewSpecularGpu.* Sources/Renderer/Gpu/ScenePrewarm.hpp Sources/Renderer/Render.cpp tests/virtual_shadow_integration_contract.cpp
git commit -m "Renderer: switch direct lighting to VSM SMRT"
```

---

### Task 8: CPU Reference Soft-Shadow Semantics

**Files:**
- Modify: `Sources/Renderer/Shadows/Shadows.hpp/.cpp`
- Modify: `Sources/Renderer/Lighting/Lighting.hpp/.cpp`
- Test: add `tests/shadow_softness_contract.cpp`

**Interfaces:**
- CPU reference does not emulate GPU virtual pages; it provides deterministic finite-source visibility semantics for RendererCheck truth scenes.
- Extend `Lighting::LightSample` with source extent copied from ECS light data.
- Add deterministic `Scene::visibility(..., std::uint64_t frame_index)` overload that samples finite source points with the same 7-ray reference count and blocker-distance/contact-hardening intent.

- [ ] **Step 1: RED contract** builds a simple blocker/receiver arrangement and verifies near-contact visibility is harder than a receiver farther behind the blocker when source extent is non-zero; zero source extent remains binary/hard.

- [ ] **Step 2: Implement deterministic disk/sphere source sampling with fixed sequence for CPU reference.** No random_device, no wall-clock seed.

- [ ] **Step 3: Preserve exact hard-shadow behavior when `source_radius==0` and `source_angle==0`.**

- [ ] **Step 4: Run contract plus existing shadow RendererCheck CPU-reference tests.**

```bash
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -ISources tests/shadow_softness_contract.cpp Sources/Renderer/Shadows/Shadows.cpp Sources/Renderer/Lighting/Lighting.cpp Sources/Renderer/Math/Math.cpp Sources/Renderer/Mesh/Mesh.cpp -o /tmp/shadow-softness
/tmp/shadow-softness
```

- [ ] **Step 5: Commit.**

```bash
git add Sources/Renderer/Shadows/Shadows.* Sources/Renderer/Lighting/Lighting.* tests/shadow_softness_contract.cpp
git commit -m "Renderer: add finite source CPU shadow reference"
```

---

### Task 9: Shadow Diagnostics, Profiler Counters, and RendererCheck Scenes

**Files:**
- Modify: `Sources/Renderer/Gpu/Profiler.hpp/.cpp`
- Modify: `Sources/Renderer/Gpu/CacheStats.hpp`
- Modify: `Sources/Renderer/Debug.cpp`
- Modify: `Sources/Renderer/Test/TestScene.cpp`
- Modify: `rendercheck.toml`
- Test: extend `tests/cache_stats_contract.cpp`
- Test: add `tests/virtual_shadow_debug_contract.cpp`

**Produces:**
- profiler pass `VirtualShadow`
- metrics: `virtual_shadow_ms`, page requested/rendered/cached/evicted, static/dynamic invalidations
- deterministic test scenes:
  - `ShadowContactHardening`
  - `ShadowSoftDirectional`
  - `ShadowSoftPoint`
  - `ShadowClipmapNear`
  - `ShadowClipmapFar`
  - `ShadowCacheCameraMove`
  - `ShadowCacheCasterMove`
  - `ShadowPoolPressure`
- debug modes expose at least page/clipmap/SMRT information through renderer-owned debug output; no new public application API.

- [ ] **Step 1: Write RED debug contract** checking all test names exist in `TestScene.cpp`/`rendercheck.toml`, profiler enum/name/metric contain `VirtualShadow`, and cache counters exist.

- [ ] **Step 2: Add test-scene source extents.** `ShadowSoftDirectional` sets a visible directional source angle; `ShadowSoftPoint` sets non-zero radius; contact-hardening scene uses two receivers at different blocker separation.

- [ ] **Step 3: Add clipmap/cache movement scenes.** Camera-only movement requests different receiver pages while world revision remains stable; caster movement changes geometry revision and invalidates affected pages.

- [ ] **Step 4: Add pool-pressure stress scene** that requests enough visible regions to exercise positive dynamic LOD bias without overflow/corruption.

- [ ] **Step 5: Wire profiler around VSM page marking/raster rather than lumping it into the full direct pass.** Preserve existing timer-query architecture.

- [ ] **Step 6: Run contracts/build/RendererCheck shadow subset.**

```bash
c++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -ISources tests/virtual_shadow_debug_contract.cpp -o /tmp/virtual-shadow-debug && /tmp/virtual-shadow-debug
c build
renderercheck run ShadowDirectional
renderercheck run ShadowBias
renderercheck run ShadowContactHardening
renderercheck run ShadowSoftDirectional
renderercheck run ShadowSoftPoint
renderercheck run ShadowClipmapNear
renderercheck run ShadowClipmapFar
renderercheck run ShadowCacheCameraMove
renderercheck run ShadowCacheCasterMove
renderercheck run ShadowPoolPressure
```

- [ ] **Step 7: Run release performance sample and record `virtual_shadow_ms` plus page-cache hit ratio.**

```bash
c build --release
RENDERCHECK_PERF=1 CRAPGAME_GPU_PROFILE=1 ./build/release/crapgame
```

- [ ] **Step 8: Commit.**

```bash
git add Sources/Renderer/Gpu/Profiler.* Sources/Renderer/Gpu/CacheStats.hpp Sources/Renderer/Debug.cpp Sources/Renderer/Test/TestScene.cpp rendercheck.toml tests/cache_stats_contract.cpp tests/virtual_shadow_debug_contract.cpp
git commit -m "Renderer: validate virtual shadow performance"
```

---

### Task 10: Remove Obsolete Monolithic Static Shadow Path + Final VSM Gate

**Files:**
- Delete: `Sources/Renderer/Gpu/StaticShadowCacheGpu.hpp/.cpp`
- Delete: `Sources/Renderer/Gpu/StaticShadowPolicy.hpp`
- Delete: `Sources/Renderer/Gpu/StaticShadowShader.hpp`
- Modify/delete assertions in: `tests/static_shadow_cache_contract.cpp`
- Search/modify any remaining consumers.

- [ ] **Step 1: Search for obsolete identifiers.**

```bash
grep -R "StaticShadowCacheGpu\|StaticShadowPolicy\|STATIC_SHADOW_\|staticShadowVisibility" Sources tests
```

Expected before cleanup: only obsolete files/tests and any missed consumer references.

- [ ] **Step 2: Remove old classes/files and replace the old contract with assertions that the active renderer contains no fixed-PCF/static-shadow path.**

```cpp
require(source.find("StaticShadowCacheGpu")==std::string::npos,"obsolete monolithic shadow cache remains active");
require(source.find("staticShadowVisibility")==std::string::npos,"obsolete PCF receiver path remains active");
```

- [ ] **Step 3: Run the complete local contract set relevant to renderer architecture.**

```bash
for test in \
  virtual_shadow_policy_contract \
  shadow_page_cache_contract \
  virtual_shadow_shader_contract \
  smrt_shadow_contract \
  virtual_shadow_integration_contract \
  shadow_softness_contract \
  virtual_shadow_debug_contract \
  cache_stats_contract \
  static_sponza_frame_policy_contract \
  scene_prewarm_contract; do
    echo "$test"
done
c build
```

Compile/run each standalone contract with the same strict C++17 flags used in its task; `c build` must exit 0.

- [ ] **Step 4: Run final shadow visual gate.**

```bash
renderercheck run ShadowPoint
renderercheck run ShadowDirectional
renderercheck run ShadowSpot
renderercheck run ShadowBias
renderercheck run ShadowContactHardening
renderercheck run ShadowSoftDirectional
renderercheck run ShadowSoftPoint
renderercheck run FinalScene
```

- [ ] **Step 5: Compare release performance against the pre-VSM baseline.** The implementation is not accepted if page marking/raster accidentally becomes a per-frame full-scene shadow redraw on static Sponza; verify cached camera/stationary behavior with profiler counters rather than visual inspection alone.

- [ ] **Step 6: Commit cleanup.**

```bash
git add -A
git commit -m "Renderer: retire monolithic PCF shadows"
```

---

## Completion Gate for This Plan

This plan is complete only when all of the following are true simultaneously:

1. Active directional shadows no longer fit the whole scene into one 2048x2048 map.
2. The active renderer uses 128x128 virtual pages, a 2048-page physical budget, hierarchical page fallback, receiver-driven page requests, and directional clipmaps 6..22.
3. Camera movement reuses/remaps cached physical pages instead of globally invalidating them.
4. Geometry/material/light changes selectively dirty shadow pages.
5. Page pressure produces bounded positive resolution LOD bias instead of unbounded allocation.
6. Shadow visibility is SMRT finite-source visibility, not fixed PCF.
7. Directional/local default sampling is 7 rays x 8 VSM depth samples with adaptive/early-out behavior and frame-index dithering.
8. Contact shadows remain hard and penumbrae widen with blocker/receiver separation.
9. Smart receiver bias, normal bias, slope extrapolation, and screen-ray start adjustment are separate controls.
10. Direct diffuse, direct specular, and dynamic/imported direct paths consume the same virtual-shadow visibility model.
11. CPU reference has deterministic finite-source shadow behavior for visual validation.
12. Profiler/debug counters expose page requested/rendered/cached/evicted and VSM GPU time.
13. Existing `Renderer::Rendering` public API and CrapGame source style remain intact.
14. No non-LWJGL-2.9.3 graphics API has been introduced.
15. Old `StaticShadowCacheGpu` / fixed-PCF code is not part of the active renderer.

After this gate, the next parity plan continues top-to-bottom through the approved deep-analysis checklist: UE direct BRDF/finite-light material response, then Surface Cache/Lumen Scene, Radiance Cache, Screen Probe Gather, radiosity, short-range AO, reflections, ReSTIR, translucency/refraction, temporal/filtering, and final performance/debug parity.
