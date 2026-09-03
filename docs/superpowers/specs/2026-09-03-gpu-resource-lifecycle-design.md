# GPU Resource Lifecycle Design

## Goal

Reduce OpenGL object churn and make ownership/reset semantics explicit without changing rendered output, pass cadence, BVH behavior, or RendererCheck visual behavior.

## Scope

The interactive GPU path only. RendererCheck's deterministic CPU reference renderer remains unchanged.

## Decisions

- A same-size resize with valid resources is a strict no-op.
- A changed-size resize reuses existing texture/FBO object names and reallocates storage in-place with `glTexImage2D`; it does not delete/recreate object names unless shutdown or recovery requires it.
- GBuffer keeps one framebuffer and its five attachment texture objects for the renderer lifetime.
- Direct Lighting keeps its direct-color texture object for the renderer lifetime.
- Lumen keeps its six temporal-history texture objects plus final-color texture object for the renderer lifetime.
- BVH node-buffer ownership belongs entirely to `DirectLightingGpu::shutdown()`. Callers never need a separate acceleration-release step.
- `DirectLightingGpu::shutdown()` resets all BVH shader/benchmark/cache flags and locations so init -> use BVH -> shutdown -> init is equivalent to a fresh object.
- `LumenGpu::shutdown()` resets BVH trace-program state for the same reason.
- Surface formats from Stage 11 are preserved exactly; no additional quality changes are part of this stage.
- No new per-commit CI. The `ci-check` branch is advanced only when the complete batch is ready.

## Verification

- Pure resize/lifecycle policy contract compiles without OpenGL/lwcgl.
- Existing performance/scheduling/surface-format contracts remain green.
- Final machine gate: release build, normal launch, `renderercheck perf FinalScene`, `renderercheck perf BVH16-bvh`, and `renderercheck perf StableScene`.
