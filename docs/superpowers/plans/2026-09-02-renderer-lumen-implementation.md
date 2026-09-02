# Renderer + Lumen Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the complete staged CrapGame renderer and software-Lumen pipeline behind `Renderer::Rendering`, with one commit per numbered stage and a full RendererCheck scene matrix.

**Architecture:** `Ecs` owns world-facing components only. `Renderer::Rendering` owns all rasterization, buffers, lighting, shadows, temporal history, distance fields, Surface Cache, probes, radiosity, AO, reflections, presentation, debug modes, and RendererCheck capture. Exact `lwcgl v2.9.3` is kept; because that branch exposes GL11 only, modern GPU-only concepts are implemented as renderer-owned CPU/software equivalents and the final RGB framebuffer is presented through the existing GL11 context.

**Tech Stack:** C++17, C-BuildSystem, lwcgl v2.9.3, OpenGL/GLU already provided by lwcgl, RendererCheck.

**Spec:** `docs/superpowers/specs/2026-09-02-renderer-lumen-design.md`

## Global Constraints
- Keep `lwcgl v2.9.3`; do not switch branches or add libraries.
- Directories/files are PascalCase under `Sources/Ecs` and `Sources/Renderer`.
- Functions are camelCase with no underscores; variables are snake_case only.
- Preserve the current CrapGame brace/spacing/wrapping style.
- `main.cpp` stays thin; renderer internals never leak outside `Renderer::Rendering`.
- Every stage must compile before its commit is moved onto `main`.
- Every renderer subsystem receives deterministic RendererCheck coverage by Stage 36.

---

### Stage 1: Rename and repair boundaries
- [ ] Rename `Sources/ECS` -> `Sources/Ecs`, `ecs.*` -> `Ecs.*`, `Sources/RENDER` -> `Sources/Renderer`, `render.*` -> `Render.*`.
- [ ] Rename namespaces to `Ecs` and `Renderer`, class `Renderer` -> `Rendering`.
- [ ] Repair the in-progress `LightComponent` syntax without expanding behavior yet.
- [ ] Update `main.cpp` and `build.c` recursive source globs.
- [ ] Verify all translation units with `-std=c++17 -Wall -Wextra -Werror -fsyntax-only`.
- [ ] Commit `Renderer Stage 01: rename renderer and ECS`.

### Stage 2: Math
- [ ] Add `Renderer/Math/Math.hpp/.cpp` with `Vec2`, `Vec3`, `Vec4`, `Mat4`, vector operations, TRS, perspective, look-at, transform, inverse-TRS helpers.
- [ ] Add deterministic math assertions.
- [ ] Verify and commit `Renderer Stage 02: add renderer math`.

### Stage 3: Renderer-owned matrices
- [ ] Move model/view/projection construction from fixed-function state to renderer math.
- [ ] Keep the same permanent camera/cube/ground appearance.
- [ ] Verify and commit `Renderer Stage 03: own camera matrices`.

### Stage 4: Indexed mesh representation
- [ ] Add `Renderer/Mesh/Mesh.hpp/.cpp` with `Vertex`, `Mesh`, cube/plane builders, triangle access, bounds.
- [ ] ECS `MeshComponent` identifies a renderer mesh primitive/handle while renderer owns mesh data.
- [ ] Verify mesh index/normal invariants and commit `Renderer Stage 04: add indexed meshes`.

### Stage 5: Renderer mesh buffers
- [ ] Add renderer-owned immutable mesh buffers and transformed-triangle cache; this is the strict-v2.9.3 software equivalent of the VBO/IBO/VAO stage.
- [ ] Remove immediate-mode cube/plane special-case drawing from scene traversal.
- [ ] Verify and commit `Renderer Stage 05: add mesh buffers`.

### Stage 6: Software shader pipeline
- [ ] Add `Renderer/Shader/Shader.hpp/.cpp` with vertex transformation, fragment material interpolation, normal handling, and debug shading entry points; this is the strict-v2.9.3 equivalent of the GLSL stage.
- [ ] Verify and commit `Renderer Stage 06: add shader pipeline`.

### Stage 7: Material ECS
- [ ] Add `MaterialComponent` with albedo, metallic, roughness, emissive color/strength.
- [ ] Attach materials to permanent cube/ground.
- [ ] Verify and commit `Renderer Stage 07: add ECS materials`.

### Stage 8: Light ECS
- [ ] Complete `LightType` and `LightComponent` for directional, point, spot, color, intensity, range, cones, shadows, indirect intensity.
- [ ] Add component storage/access and permanent point-light entity.
- [ ] Verify and commit `Renderer Stage 08: add ECS lights`.

### Stage 9: GBuffer
- [ ] Add `Renderer/GBuffer/GBuffer.hpp/.cpp` with CPU depth, position, normal, albedo, material and entity buffers plus triangle rasterization.
- [ ] Verify depth/order tests and commit `Renderer Stage 09: add GBuffer`.

### Stage 10: PBR BRDF
- [ ] Add Lambert diffuse, GGX NDF, Smith visibility, Schlick Fresnel and energy-conserving material evaluation in `Renderer/Lighting`.
- [ ] Verify numeric BRDF bounds and commit `Renderer Stage 10: add PBR BRDF`.

### Stage 11: Direct ECS lighting
- [ ] Shade GBuffer using point/directional/spot lights with attenuation and cone falloff.
- [ ] Verify isolated light-type outputs and commit `Renderer Stage 11: add direct lighting`.

### Stage 12: Shadows
- [ ] Add `Renderer/Shadows/Shadows.hpp/.cpp` with ray/triangle visibility for all light types, bias handling, and shadow factor output.
- [ ] Verify occluder/no-occluder cases and commit `Renderer Stage 12: add shadows`.

### Stage 13: Previous-frame state
- [ ] Cache previous camera and entity transforms inside `Rendering`.
- [ ] Verify first-frame/current-frame behavior and commit `Renderer Stage 13: add previous frame state`.

### Stage 14: Motion vectors
- [ ] Produce per-pixel motion vectors from previous/current clip positions.
- [ ] Verify static zero motion and moving-cube motion; commit `Renderer Stage 14: add motion vectors`.

### Stage 15: Temporal history
- [ ] Add history color/depth/normal validity buffers and reprojection rejection.
- [ ] Verify disocclusion rejection and commit `Renderer Stage 15: add temporal history`.

### Stage 16: TAA
- [ ] Add jitter sequence, history resolve, neighborhood clamp, and deterministic TAA.
- [ ] Verify static convergence/motion rejection and commit `Renderer Stage 16: add TAA`.

### Stage 17: Screen-space tracing
- [ ] Add `Renderer/Lumen/ScreenTrace.hpp/.cpp` depth-buffer ray marching with hit/miss data.
- [ ] Verify forced hit/miss cases and commit `Renderer Stage 17: add screen tracing`.

### Stage 18: Mesh distance fields
- [ ] Add `Renderer/Lumen/DistanceField.hpp/.cpp` local SDF volumes, triangle distance sampling, sign classification, trilinear sampling.
- [ ] Generate fields for cube/ground meshes.
- [ ] Verify known inside/outside distances and commit `Renderer Stage 18: add mesh SDFs`.

### Stage 19: SDF sphere tracing
- [ ] Add local/world distance sampling and sphere-trace hit/miss traversal.
- [ ] Verify cube hit/miss rays and commit `Renderer Stage 19: add SDF tracing`.

### Stage 20: Global SDF
- [ ] Add camera-centered clipmap/grid aggregation for scene distance queries.
- [ ] Verify nearest-surface samples and commit `Renderer Stage 20: add Global SDF`.

### Stage 21: Unified Lumen trace
- [ ] Add screen-first then SDF-fallback trace path with source tagging.
- [ ] Verify off-screen fallback and commit `Renderer Stage 21: unify Lumen tracing`.

### Stage 22: Mesh Cards
- [ ] Add six-direction Card descriptions/capture samples per renderable mesh.
- [ ] Verify cube face coverage and commit `Renderer Stage 22: add Lumen Cards`.

### Stage 23: Surface Cache
- [ ] Add `Renderer/Lumen/SurfaceCache.hpp/.cpp` material/depth/normal/card cache entries with entity mapping.
- [ ] Verify material cache lookup and commit `Renderer Stage 23: add Surface Cache`.

### Stage 24: Lumen scene lighting
- [ ] Inject ECS direct light into Surface Cache records with shadow visibility.
- [ ] Verify direct cache response to light movement and commit `Renderer Stage 24: light Surface Cache`.

### Stage 25: World radiance cache
- [ ] Add `Renderer/Lumen/RadianceCache.hpp/.cpp` 3D probe grid, directional radiance samples, interpolation and bounded updates.
- [ ] Verify probe interpolation and commit `Renderer Stage 25: add Radiance Cache`.

### Stage 26: Screen Probe Gather
- [ ] Add `Renderer/Lumen/ScreenProbe.hpp/.cpp` screen-grid probe placement, tracing, radiance gather and full-resolution interpolation.
- [ ] Verify placement/gather tests and commit `Renderer Stage 26: add Screen Probe Gather`.

### Stage 27: Importance sampling
- [ ] Bias probe directions using surface normal and previous radiance while preserving deterministic sequence.
- [ ] Verify hemisphere/orientation constraints and commit `Renderer Stage 27: add probe importance sampling`.

### Stage 28: Temporal GI
- [ ] Add motion-aware GI history, depth/normal rejection and accumulation.
- [ ] Verify convergence/rejection and commit `Renderer Stage 28: add temporal GI`.

### Stage 29: Radiosity
- [ ] Add `Renderer/Lumen/Radiosity.hpp/.cpp` Surface Cache feedback for bounded multi-bounce indirect lighting.
- [ ] Verify one-bounce vs multi-bounce energy and commit `Renderer Stage 29: add radiosity`.

### Stage 30: Short-range AO
- [ ] Add `Renderer/Lumen/ShortRangeAo.hpp/.cpp` local SDF hemisphere occlusion/contact GI.
- [ ] Verify cube/ground contact and commit `Renderer Stage 30: add short range AO`.

### Stage 31: Rough reflections
- [ ] Add `Renderer/Lumen/Reflections.hpp/.cpp` rough specular from radiance cache/probes.
- [ ] Verify roughness response and commit `Renderer Stage 31: add rough reflections`.

### Stage 32: Smooth reflections
- [ ] Add reflected screen ray then SDF fallback hit lighting for smooth materials.
- [ ] Verify on-screen/off-screen reflection tests and commit `Renderer Stage 32: add smooth reflections`.

### Stage 33: Reflection filtering
- [ ] Add temporal/spatial reflection resolve with hit validity/depth/normal checks.
- [ ] Verify static convergence/motion rejection and commit `Renderer Stage 33: filter reflections`.

### Stage 34: Dirty regions
- [ ] Track ECS transform/material/light snapshots and invalidate only affected SDF/cache/probe/history state.
- [ ] Verify moving-light/moving-object invalidation and commit `Renderer Stage 34: add dirty tracking`.

### Stage 35: Update budgets
- [ ] Add deterministic per-frame budgets for distance fields, Surface Cache records, radiance probes, screen probes and radiosity updates.
- [ ] Verify budget caps and commit `Renderer Stage 35: add update budgets`.

### Stage 36: RendererCheck and debug suite
- [ ] Add `Renderer/Test/TestScene.hpp/.cpp` deterministic scene selection from `RENDERCHECK_TEST`.
- [ ] Add intermediate `RenderMode` output for GBuffer, direct light, shadow, SDF, Surface Cache, probes, GI, AO and reflections.
- [ ] Expand `rendercheck.toml` to the complete specified test matrix with convergence warmups.
- [ ] Capture directly from renderer-owned RGB buffer before swap.
- [ ] Verify test-name mapping is complete/unknown names fail explicitly; syntax-check all sources.
- [ ] Commit `Renderer Stage 36: add RendererCheck Lumen suite`.

## Completion verification
- [ ] Run `c build` with installed lwcgl v2.9.3 and RendererCheck.
- [ ] Run `renderercheck run` and approve baselines only after visual inspection.
- [ ] Confirm normal execution still uses only `renderer.render(world)` for the render pipeline.
