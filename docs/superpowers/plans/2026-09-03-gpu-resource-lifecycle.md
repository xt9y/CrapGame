# GPU Resource Lifecycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove avoidable GPU object recreation during resize and make BVH/Lumen resource reset ownership correct across shutdown/re-init.

**Architecture:** Add a tiny OpenGL-independent resize/lifecycle policy contract, then make GBuffer, Direct Lighting, and Lumen reuse existing GL object names while reallocating storage in-place. Direct Lighting becomes the sole owner of BVH buffer destruction and both Direct/Lumen reset shader-mode state fully on shutdown.

**Tech Stack:** C++17, OpenGL 4.3 through lwcgl v2.9.3, RendererCheck performance cases.

**Spec:** `docs/superpowers/specs/2026-09-03-gpu-resource-lifecycle-design.md`

## Global Constraints

- Interactive runtime remains OpenGL 4.3 / lwcgl v2.9.3.
- RendererCheck CPU reference path is untouched.
- No visual-quality or scheduling changes.
- Preserve Stage 11 surface formats exactly.
- Work directly on `main`.
- Do not run per-commit GitHub Actions; advance `ci-check` only after the complete batch.

---

### Task 1: Resize policy contract

**Files:**
- Create: `Sources/Renderer/Gpu/ResourceLifecycle.hpp`
- Create: `tests/gpu_resource_lifecycle_contract.cpp`
- Modify: `.github/workflows/performance-metrics-contract.yml`

**Interfaces:**
- Produces: `normalizedExtent(int)`, `resizeStorageRequired(int,int,bool,int,int)`.

- [ ] **Step 1: Write the failing contract**

Assert that valid same-size resources require no storage operation, changed dimensions require one, invalid resources require one, and zero/negative requested dimensions normalize to 1.

- [ ] **Step 2: Verify RED offline**

Compile the contract before creating `ResourceLifecycle.hpp`; expected failure is missing header.

- [ ] **Step 3: Implement the minimal pure policy**

Create header-only C++17 helpers with no GL/lwcgl dependencies.

- [ ] **Step 4: Verify GREEN offline**

Compile with `-Wall -Wextra -Wpedantic -Werror` and run the contract.

- [ ] **Step 5: Add the contract to the gated `ci-check` workflow**

Do not advance `ci-check` yet.

### Task 2: Reuse GBuffer objects across resize

**Files:**
- Modify: `Sources/Renderer/Gpu/GBufferGpu.cpp`
- Modify: `Sources/Renderer/Gpu/GBufferGpu.hpp` only if helper signatures require it.

**Interfaces:**
- Consumes: `resizeStorageRequired` and Stage 11 `SurfaceFormat` constants.
- Produces: persistent framebuffer/attachment object names across dimension changes.

- [ ] **Step 1: Replace create/delete-on-resize helpers with ensure-storage helpers**

Create texture names only when zero; otherwise reuse existing names and call `glTexImage2D` for the new extent/format.

- [ ] **Step 2: Preserve the existing FBO object**

Generate `framebuffer_` only if zero; reattach persistent texture names after storage changes.

- [ ] **Step 3: Remove `destroyAttachments()` from normal resize**

Keep it for shutdown/error recovery only.

### Task 3: Reuse Direct/Lumen texture objects

**Files:**
- Modify: `Sources/Renderer/Gpu/DirectLightingGpu.cpp`
- Modify: `Sources/Renderer/Gpu/LumenGpu.cpp`

**Interfaces:**
- Consumes: `resizeStorageRequired`, Stage 11 surface formats.
- Produces: persistent Direct and Lumen texture object names across resizes.

- [ ] **Step 1: Direct resize**

Reuse `direct_color_`; allocate a texture object only if it is zero, and re-specify storage in-place on extent changes.

- [ ] **Step 2: Lumen resize**

Reuse all history/final texture object names and re-specify their storage in-place. Reset temporal validity/index whenever dimensions change.

- [ ] **Step 3: Keep same-size ready resizes strict no-ops**

Use the shared pure policy instead of duplicating ad-hoc conditions.

### Task 4: Correct BVH/Lumen shutdown ownership

**Files:**
- Modify: `Sources/Renderer/Gpu/DirectLightingGpu.hpp`
- Modify: `Sources/Renderer/Gpu/DirectLightingGpu.cpp`
- Modify: `Sources/Renderer/Gpu/LumenGpu.cpp`

**Interfaces:**
- Produces: fresh-state semantics after shutdown/re-init.

- [ ] **Step 1: Direct shutdown owns BVH destruction**

Call `releaseAcceleration()` from `DirectLightingGpu::shutdown()` before clearing ordinary scene buffers.

- [ ] **Step 2: Reset Direct mode/cache state**

Reset BVH uniform location, primitive count, shader validation/active flags, benchmark config/report flags, and `use_bvh_`.

- [ ] **Step 3: Reset Lumen mode/cache state**

Reset BVH trace uniform location, shader validation/active flags, history state, dimensions, and program locations.

### Task 5: Final verification boundary

**Files:** none unless verification exposes defects.

- [ ] **Step 1: Offline contracts**

Run performance metrics, GPU scheduling, surface formats, and resource lifecycle contracts locally/offline where possible.

- [ ] **Step 2: Source consistency review**

Confirm normal resize no longer deletes GL texture/FBO objects and shutdown remains the only destruction path.

- [ ] **Step 3: Advance `ci-check` once**

Wait for the single gated contract workflow and inspect its result.

- [ ] **Step 4: Machine integration gate**

User runs `c build --release`, normal app launch, `renderercheck perf FinalScene`, `renderercheck perf BVH16-bvh`, and `renderercheck perf StableScene`.
