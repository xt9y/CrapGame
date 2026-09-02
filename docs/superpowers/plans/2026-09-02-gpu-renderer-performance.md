# GPU Renderer Performance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace CrapGame's CPU-bound minute-per-frame renderer with a persistent OpenGL 4.3 GPU pipeline while keeping lwcgl 2.9.3 compatibility and the 67 RendererCheck outputs.

**Architecture:** Extend `xt9y/lwcgl` `v2.9.3` with the LWJGL-2-era modern OpenGL subset required by CrapGame, then migrate renderer passes in dependency order. Keep renderer-owned deterministic capture only in RendererCheck mode and remove normal-frame readbacks/synchronization.

**Tech Stack:** C11 lwcgl/GLFW loader, OpenGL 4.3 compatibility profile, GLSL 430 compute/vertex/fragment shaders, C++17 CrapGame, C-BuildSystem, RendererCheck.

**Spec:** `docs/superpowers/specs/2026-09-02-gpu-renderer-performance-design.md`

## Global Constraints

- Keep CrapGame on `lwcgl` branch `v2.9.3`.
- Linux preferred context is OpenGL 4.3 compatibility profile.
- Do not silently emulate missing GL43 GPU functionality with the old minute-per-frame path.
- Interactive rendering must not call `glFinish()` or read the final framebuffer back to the CPU.
- RendererCheck capture remains deterministic and keeps all 67 test/output names.
- Temporal history survives ordinary frames and is reset only when required.
- Use persistent GPU resources and dirty-region/frame-budget updates.

---

### Task 1: lwcgl modern-context and GL capability contract

**Files:**
- Create in `xt9y/lwcgl`: `tests/modern_gl_contract.cpp`
- Create in `xt9y/lwcgl`: `include/lwcgl/glmodern.h`
- Create in `xt9y/lwcgl`: `src/glmodern.c`
- Modify in `xt9y/lwcgl`: `include/lwcgl/lwcgl.h`
- Modify in `xt9y/lwcgl`: `src/lwcgl.c`
- Modify in `xt9y/lwcgl`: `Makefile`

**Interfaces:**
- Produces `Display.setContextVersion(int,int)`, `Display.setContextProfile(int)`, `lwcglLoadModernGL()`, `lwcglModernGLAvailable()`.
- Produces GL15/GL20/GL30/GL42/GL43 function-pointer API groups used by renderer code.

- [ ] Write compile contract referring to the required API before implementing it.
- [ ] Verify the old v2.9.3 headers cannot compile that contract.
- [ ] Add context request state and apply GLFW version/profile hints before window creation.
- [ ] Add `glfwGetProcAddress` loader and explicit missing-function diagnostics.
- [ ] Add buffer/shader/FBO/VAO/image/compute/timer-query functions required by CrapGame.
- [ ] Extend `make check` to compile the legacy and modern contracts.
- [ ] Build with strict C11/C++17 warnings and commit to `v2.9.3`.

### Task 2: Remove unconditional frame stalls and history destruction

**Files:**
- Modify: `Sources/main.cpp`
- Modify: `Sources/Renderer/Render.cpp`
- Modify: `Sources/Renderer/GBuffer/GBuffer.cpp`
- Modify: `Sources/Renderer/Temporal/Temporal.cpp`
- Test: `Sources/Renderer/Test/TestScene.cpp` / RendererCheck temporal scenes

**Interfaces:**
- `Rendering::resize()` becomes a no-op when dimensions are unchanged.
- `glFinish()` executes only for deterministic capture when required.

- [ ] Add a regression contract for history surviving an unchanged resize.
- [ ] Make unchanged `resize()` return before touching buffers/history.
- [ ] Remove interactive `glFinish()`.
- [ ] Run/build RendererCheck temporal scenes and commit.

### Task 3: GPU device and persistent resource layer

**Files:**
- Create: `Sources/Renderer/Gpu/Gpu.hpp`
- Create: `Sources/Renderer/Gpu/Gpu.cpp`
- Create: `Sources/Renderer/Gpu/Shader.hpp`
- Create: `Sources/Renderer/Gpu/Shader.cpp`
- Modify: `build.c`
- Modify: `Sources/Renderer/Render.hpp`

**Interfaces:**
- Shader compile/link helpers with complete info-log errors.
- RAII-ish explicit lifetime wrappers for buffers, textures, FBOs, VAOs and timer queries.
- capability check requiring GL43 compute, SSBO, image load/store and FBO support.

- [ ] Add failing compile/runtime contracts for resource creation and shader failure reporting.
- [ ] Implement minimal wrappers.
- [ ] Add renderer initialization capability gate.
- [ ] Commit.

### Task 4: GPU GBuffer and batched geometry

**Files:**
- Create: `Sources/Renderer/Gpu/GBufferGpu.hpp/.cpp`
- Create: `Sources/Renderer/Gpu/Shaders/gbuffer.vert`
- Create: `Sources/Renderer/Gpu/Shaders/gbuffer.frag`
- Modify: `Sources/Renderer/Render.cpp`
- Modify: `Sources/Renderer/Mesh/*` as needed for persistent VBO/IBO upload.

**Interfaces:**
- Persistent MRT textures for world position/depth, normal/material, albedo/emissive and motion/entity metadata.
- Persistent cube/plane VBO+IBO data and instance updates without re-uploading static mesh topology.

- [ ] Add GBuffer RendererCheck contracts.
- [ ] Create MRT FBO and resize only on resolution changes.
- [ ] Upload static mesh buffers once.
- [ ] Batch/instance visible ECS geometry.
- [ ] Match albedo/normal/depth/material/motion debug outputs.
- [ ] Commit.

### Task 5: GPU shadows and direct lighting

**Files:**
- Create: `Sources/Renderer/Gpu/ShadowsGpu.hpp/.cpp`
- Create: direct-light compute shader and shadow depth shaders.
- Modify: `Sources/Renderer/Render.cpp`

**Interfaces:**
- Dirty shadow maps replace CPU `pixels × lights × triangles` tests.
- Direct-light compute writes an HDR image from GBuffer + light buffer + shadow textures.

- [ ] Add direct/shadow RendererCheck contracts.
- [ ] Implement light SSBO.
- [ ] Implement shadow-map invalidation on geometry/light changes.
- [ ] Implement direct-light compute dispatch.
- [ ] Match debug outputs and commit.

### Task 6: GPU scene-distance representation

**Files:**
- Create: `Sources/Renderer/Gpu/SdfGpu.hpp/.cpp`
- Add Global-SDF compute/update shaders.
- Modify scene-change integration.

**Interfaces:**
- Mesh SDF templates uploaded once.
- Instance transforms live in SSBOs.
- camera-centered Global SDF clipmaps are 3D textures updated only when their snapped center/dirty geometry requires it.

- [ ] Add Mesh/Global SDF RendererCheck contracts.
- [ ] Upload template fields once.
- [ ] Implement compute clipmap rebuild/update.
- [ ] Eliminate per-frame CPU global clipmap voxel loops.
- [ ] Commit.

### Task 7: GPU Surface Cache and Radiance Cache

**Files:**
- Create GPU cache modules/shaders under `Sources/Renderer/Gpu/`.
- Modify Lumen orchestration.

**Interfaces:**
- Card/surface metadata stored persistently in SSBO/texture resources.
- lighting/radiosity/radiance cache updates are budgeted compute dispatches.

- [ ] Add cache debug-output contracts.
- [ ] Implement dirty card/surface updates.
- [ ] Implement radiance cache work queue bounded per frame.
- [ ] Match debug modes and commit.

### Task 8: GPU Screen Probe Gather, GI and short-range AO

**Files:**
- Add half-resolution screen-probe, GI and AO compute shaders.
- Modify renderer orchestration/history resources.

**Interfaces:**
- No per-valid-pixel CPU unified traces.
- Probe tracing and contact AO execute on GPU at reduced Lumen resolution and feed persistent temporal reconstruction.

- [ ] Add screen-probe/indirect/AO contracts.
- [ ] Dispatch probe rays in compute workgroups.
- [ ] Reconstruct/denoise using GBuffer edge weights.
- [ ] Preserve temporal history across frames.
- [ ] Commit.

### Task 9: GPU reflections and temporal reconstruction

**Files:**
- Add reflection trace/resolve compute shaders.
- Modify temporal resource ownership.

**Interfaces:**
- smooth reflections use GPU screen/SDF tracing.
- rough reflections use radiance-cache sampling.
- temporal resolve runs entirely on GPU.

- [ ] Add reflection/off-screen fallback/motion contracts.
- [ ] Implement GPU trace and fallback.
- [ ] Implement GPU temporal reflection resolve.
- [ ] Commit.

### Task 10: Final composite, present and RendererCheck capture

**Files:**
- Add composite shader.
- Modify `Sources/Renderer/Render.cpp`, `Debug.cpp`, capture path.

**Interfaces:**
- Final HDR composite stays GPU-resident through presentation.
- only capture mode performs readback into `color_buffer_`.

- [ ] Add final/debug capture contracts.
- [ ] Remove `glDrawPixels` from normal presentation.
- [ ] Present via fullscreen triangle.
- [ ] Read back only the selected RendererCheck output.
- [ ] Verify all 67 mappings and commit.

### Task 11: Profiling, adaptive budgets and cleanup

**Files:**
- Create `Sources/Renderer/Gpu/Profiler.hpp/.cpp`.
- Modify `Sources/Renderer/Lumen/Budget.*` and renderer orchestration.

**Interfaces:**
- CPU and GPU pass timings are observable.
- dynamic Lumen work budgets respond to measured GPU cost without changing deterministic RendererCheck settings.

- [ ] Add timer-query contract.
- [ ] Instrument geometry/shadows/direct/GI/reflection/composite.
- [ ] Add budget hysteresis and bounds.
- [ ] Remove dead CPU hot paths only after parity is proven.
- [ ] Commit.

### Task 12: Final verification

- [ ] Build/install lwcgl v2.9.3 with `make clean && make && make check && sudo make install`.
- [ ] Build CrapGame with `c build`.
- [ ] Run interactive scene and record CPU/GPU pass timings.
- [ ] Run all 67 `renderercheck run` captures.
- [ ] Verify no normal-frame `glFinish`, framebuffer readback, full GBuffer/history resize, CPU per-pixel shadow ray loop, CPU per-pixel AO trace loop or CPU per-pixel reflection trace loop remains.
- [ ] Commit final cleanup/report.
