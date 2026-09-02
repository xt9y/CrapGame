# GPU Renderer Performance Design

## Goal

Replace the current minute-per-frame CPU execution model with a GPU-first renderer while preserving the existing renderer feature set and RendererCheck coverage.

## Compatibility decision

CrapGame stays on `lwcgl` branch `v2.9.3`. LWJGL 2.9.3 exposes modern OpenGL bindings including GL43 compute shaders, so the compatibility version does not require an OpenGL 2.1 renderer. `lwcgl` will gain the native context/capability and GL15/GL20/GL30/GL42/GL43 subset required by CrapGame.

On Linux the preferred context is OpenGL 4.3 compatibility profile. Compatibility profile preserves the legacy GL11 surface while enabling compute shaders, SSBOs, image load/store, framebuffer objects and timer queries. If GL 4.3 is unavailable, initialization reports the missing capability explicitly instead of silently running the expensive GPU renderer on unsupported functions.

## Root causes being removed

1. `renderer.resize()` currently runs every frame. GBuffer and temporal history resize paths clear full-frame storage and invalidate temporal history.
2. Interactive rendering calls `glFinish()` every frame, forcing a CPU/GPU synchronization point.
3. Direct shadows are CPU `pixels × lights × triangles` ray tests.
4. short-range AO launches four unified traces per valid pixel.
5. smooth reflections launch a unified trace per eligible pixel.
6. unified CPU traces can perform screen tracing, up to 96 Global-SDF steps, local SDF tracing and a full SDF fallback.
7. the software GBuffer rasterizer shades every covered pixel on the CPU.
8. several full-resolution temporary vectors are allocated/assigned inside frame passes.

## Target architecture

### lwcgl v2.9.3

Add a modern OpenGL loader backed by `glfwGetProcAddress` after the Display context becomes current. The public API exposes only the capabilities needed by CrapGame, grouped by the LWJGL classes they correspond to: GL15 buffer objects, GL20 shaders/MRT, GL30 FBO/VAO, GL42 image access/memory barriers, and GL43 compute/SSBO functionality. Display gains requested context version/profile configuration while its existing `create()` entry point remains valid.

### Renderer

`Rendering` becomes an orchestration layer over persistent GPU resources. Resource allocation happens only on initialization or an actual size change.

The main GPU data path is:

1. ECS visible instances -> persistent mesh/instance buffers.
2. GPU geometry pass -> GBuffer textures and depth.
3. shadow-map pass -> GPU depth maps.
4. direct-light compute -> direct-light image.
5. scene-change updates -> persistent Global SDF / Surface Cache / Radiance Cache GPU resources.
6. screen-probe/short-range AO compute at reduced Lumen resolution -> indirect-light image.
7. reflection compute -> reflection image.
8. temporal resolve/composite compute -> final HDR image.
9. fullscreen present -> window backbuffer.

No normal interactive frame performs a framebuffer readback or `glFinish()`.

### Temporal and update policy

Temporal history persists until an actual resolution change or explicit invalidation. Geometry/material/light/camera changes update only resources they affect. Expensive Lumen resources are amortized by dirty regions and frame budgets instead of being rebuilt globally every frame.

### RendererCheck

RendererCheck remains deterministic. In capture mode only, the final/debug GPU image is read into the renderer-owned RGB capture buffer after explicit synchronization. Debug modes keep the existing 67 scene/output mappings. Interactive rendering never pays this readback cost.

## Performance policy

- GPU-first for all pixel-proportional work.
- persistent allocations; no per-frame full-image vector construction in the GPU path.
- no unconditional synchronization.
- Lumen defaults to half-resolution compute with temporal reconstruction; final composite remains display resolution.
- shadow maps and caches update only when dirty.
- use GPU timer queries plus CPU frame timers so regressions can be attributed to a pass.
- preserve an explicit software/debug fallback only for unsupported hardware/testing, not as the normal path.

## Verification

Each migration step adds a compile/runtime contract before production code. Required final verification on the target machine is:

```bash
# lwcgl v2.9.3
make clean && make && make check
sudo make install

# CrapGame
c build
c build run
renderercheck run
```

The final RendererCheck run must preserve all 67 configured captures before the CPU implementations are removed.
