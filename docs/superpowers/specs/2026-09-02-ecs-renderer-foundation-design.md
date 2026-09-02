# ECS and Renderer Foundation Design

## Goal
Create the permanent pre-Lumen rendering baseline for CrapGame while keeping lwcgl 2.9.3 as the only graphics/window/input dependency.

## Architecture
`main.cpp` owns application lifetime and scene creation. `ecs::World` owns entities and component state. `render::Renderer` reads ECS camera, transform, and renderable components and issues basic OpenGL rendering through lwcgl.

## Components
- `TransformComponent`: position, Euler rotation, scale.
- `CameraComponent`: field of view, near/far planes, active flag.
- `RenderableComponent`: primitive type and color.
- `World`: entity creation, per-component storage, component lookup, active-camera selection.

## Scene
The baseline scene contains an ECS camera, a white cube tilted so a body-diagonal corner points downward and spinning around world-up, and a large ground plane.

## Renderer
The initial renderer provides initialization, resizing, camera setup, ECS iteration, primitive drawing, depth testing, and shutdown. It deliberately contains no Lumen, PBR, custom shader system, material system, shadows, or post-processing.

## Constraints
- Keep lwcgl 2.9.3.
- Add no external libraries.
- Preserve RendererCheck framebuffer capture.
- Keep the scene deterministic by advancing cube rotation by a fixed amount per rendered frame.
