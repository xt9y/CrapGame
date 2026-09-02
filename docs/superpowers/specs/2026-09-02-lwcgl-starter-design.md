# lwcgl v2.9.3 Starter Design

## Goal

Create a minimal CrapGame starter that proves the same `lwcgl` v2.9.3 C ABI works from both C and C++ while using C-BuildSystem for builds and RendererCheck for deterministic framebuffer regression tests.

## Structure

- `main.c`: C11 example using `Display`, `Keyboard`, `Mouse`, legacy OpenGL, and RendererCheck capture helpers.
- `main.cpp`: C++17 example consuming the same C header/API directly and using the LWJGL-2-shaped `new DisplayMode(...)` syntax supported by lwcgl.
- `build.c`: two C-BuildSystem executable targets, `crapgame-c` and `crapgame-cpp`.
- `rendercheck.toml`: two visual tests, one per executable.
- `scripts/rendercheck-run.sh`: dispatches RendererCheck test arguments to the correct binary without requiring executable file mode.
- `README.md`: prerequisites and exact build/run/test commands.
- `.gitignore`: build, C-BuildSystem dependency/cache, and RendererCheck transient output exclusions while preserving approved baselines.

## Dependencies

C-BuildSystem owns dependency resolution. `lwcgl` is a source dependency from `https://github.com/xt9y/lwcgl.git` pinned to ref `v2.9.3`. RendererCheck is a header-only build dependency from `https://github.com/xt9y/RendererCheck.git` at `main` for `<rendercheck/capture.h>`.

Both targets link the native libraries required by lwcgl v2.9.3: GLFW, OpenGL and GLU, plus the Linux system libraries used by lwcgl. macOS uses the OpenGL, Cocoa, IOKit and CoreVideo frameworks and common Homebrew include/library prefixes.

## Runtime behavior

Both programs create a 640x360 LWJGL-2-style display, initialize keyboard and mouse input, and render the same deterministic immediate-mode OpenGL frame. Normal execution remains interactive until Escape or window close.

When launched by RendererCheck, each program reads RendererCheck's capture/frame-limit environment through `<rendercheck/capture.h>`, captures the back buffer on the requested frame, writes the PPM capture, and exits after the requested frame budget.

## RendererCheck workflow

The initial repository intentionally contains no approved image baselines. First-time setup is:

```sh
c build crapgame-c
c build crapgame-cpp
renderercheck approve c
renderercheck approve cpp
renderercheck run
```

Approved baselines live under `rendercheck/baselines/` and should be committed after approval.

## Success criteria

1. `main.c` is valid C11 and uses the lwcgl v2.9.3 API directly.
2. `main.cpp` is valid C++17 and uses the exact same C ABI without a C++ facade.
3. Both targets are defined only through C-BuildSystem.
4. Both targets render the same deterministic scene and support RendererCheck capture.
5. No second build system is introduced.