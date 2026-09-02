# ECS and Renderer Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first ECS-owned 3D scene and basic renderer for CrapGame.

**Architecture:** Keep application lifetime in `main.cpp`, store scene state in `ecs::World`, and make `render::Renderer` consume ECS components through lwcgl/OpenGL. The renderer remains deliberately minimal so later lighting work has a clean foundation.

**Tech Stack:** C++17, C-BuildSystem, lwcgl 2.9.3, OpenGL/GLU already provided by lwcgl, RendererCheck.

**Spec:** `docs/superpowers/specs/2026-09-02-ecs-renderer-foundation-design.md`

## Global Constraints
- Keep lwcgl 2.9.3.
- Add no external libraries.
- Preserve RendererCheck framebuffer capture.
- Do not implement Lumen yet.

---

### Task 1: ECS core

**Files:**
- Create: `Sources/ECS/ecs.hpp`
- Create: `Sources/ECS/ecs.cpp`

**Interfaces:**
- Produces `ecs::World`, `ecs::Entity`, `TransformComponent`, `CameraComponent`, `RenderableComponent`, and `Primitive`.

- [x] Define entity and component types.
- [x] Add separate optional component stores indexed by entity ID.
- [x] Add component creation/lookup and active-camera selection.
- [x] Compile-test entity/component behavior with strict warnings.

### Task 2: Renderer core

**Files:**
- Create: `Sources/RENDER/render.hpp`
- Create: `Sources/RENDER/render.cpp`

**Interfaces:**
- Consumes `const ecs::World&`.
- Produces `render::Renderer::{init,resize,render,shutdown}`.

- [x] Enable depth testing and configure the clear state.
- [x] Build projection/view state from the active ECS camera.
- [x] Iterate ECS renderables and apply their transforms.
- [x] Add cube and plane primitive drawing.
- [x] Compile-check the renderer with strict warnings.

### Task 3: Baseline scene and build wiring

**Files:**
- Modify: `Sources/main.cpp`
- Modify: `build.c`

**Interfaces:**
- Consumes ECS and Renderer interfaces from Tasks 1 and 2.

- [x] Move the ECS structs out of `main.cpp`.
- [x] Create ECS entities for camera, white tilted cube, and ground plane.
- [x] Rotate the cube by a fixed amount per frame.
- [x] Keep RendererCheck capture after rendering and before buffer swap.
- [x] Add `Sources`, `Sources/ECS/*.cpp`, and `Sources/RENDER/*.cpp` to the build.
- [x] Compile-check all C++ translation units with strict warnings.
