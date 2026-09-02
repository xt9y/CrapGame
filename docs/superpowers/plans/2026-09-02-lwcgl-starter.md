# lwcgl v2.9.3 Starter Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a minimal C/C++ starter proving lwcgl v2.9.3 works through the same C ABI in both languages, built by C-BuildSystem and visually exercised by RendererCheck.

**Architecture:** C-BuildSystem defines two standalone executables sharing source dependencies for lwcgl and RendererCheck. Each executable owns a tiny render loop and RendererCheck framebuffer capture path; a shell dispatcher lets one RendererCheck config select either binary.

**Tech Stack:** C11, C++17, C-BuildSystem, lwcgl v2.9.3, GLFW 3, legacy OpenGL 2.1, GLU, RendererCheck.

**Spec:** `docs/superpowers/specs/2026-09-02-lwcgl-starter-design.md`

## Global Constraints

- Use `https://github.com/xt9y/lwcgl.git` ref `v2.9.3`.
- Use C-BuildSystem as the only build system.
- Keep the examples as root `main.c` and root `main.cpp`.
- C and C++ must consume the same lwcgl C ABI.
- RendererCheck must be able to launch and capture both examples.

---

### Task 1: Build description

**Files:**
- Create: `build.c`

**Interfaces:**
- Produces: executables `build/debug/crapgame-c` and `build/debug/crapgame-cpp`.

- [ ] Define lwcgl as a `c_git` source dependency pinned to `v2.9.3`, include `include`, compile `src/*.c`, and set `_POSIX_C_SOURCE=200809L` for dependency sources.
- [ ] Define RendererCheck as a header-only `c_git` dependency using `include`.
- [ ] Define C11 and C++17 executable targets.
- [ ] Link GLFW/OpenGL/GLU and platform libraries required by lwcgl.

### Task 2: C and C++ examples

**Files:**
- Create: `main.c`
- Create: `main.cpp`

**Interfaces:**
- Consumes: `<lwcgl/lwcgl.h>` and `<rendercheck/capture.h>`.
- Produces: interactive render loops with deterministic RendererCheck capture.

- [ ] Implement the C version with a stack `DisplayMode`, `Display`, `Keyboard`, `Mouse`, immediate-mode OpenGL, framebuffer capture, vertical row flip, and clean teardown.
- [ ] Implement the C++ version against the same C ABI, using `new DisplayMode(...)` and `std::vector<unsigned char>` for capture storage.
- [ ] Keep the rendered scene identical in both binaries.

### Task 3: RendererCheck integration and usage docs

**Files:**
- Create: `scripts/rendercheck-run.sh`
- Create: `rendercheck.toml`
- Create: `README.md`
- Create: `.gitignore`

**Interfaces:**
- `renderercheck run c` launches `./build/debug/crapgame-c`.
- `renderercheck run cpp` launches `./build/debug/crapgame-cpp`.

- [ ] Add dispatcher with strict argument validation.
- [ ] Add two capture tests with the same warmup and pixel thresholds.
- [ ] Document prerequisites, build/run commands, and first baseline approval.
- [ ] Ignore generated build/dependency/report output without ignoring approved baselines.

### Task 4: Verification

- [ ] Confirm the build description matches current C-BuildSystem dependency APIs.
- [ ] Confirm all lwcgl calls exist on `v2.9.3`.
- [ ] Confirm RendererCheck config keys and capture helper calls match current RendererCheck.
- [ ] Fetch the resulting repository files after writes and inspect them for consistency.