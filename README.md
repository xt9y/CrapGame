# CrapGame

Minimal `lwcgl` **v2.9.3** starter showing the same LWJGL-2-style C ABI from both C and C++.

## Files

- `main.c` — C11 example
- `main.cpp` — C++17 example using the same `<lwcgl/lwcgl.h>` C ABI
- `build.c` — C-BuildSystem configuration
- `rendercheck.toml` — RendererCheck tests for both executables

Both examples create a 640x360 OpenGL 2.1 window, initialize `Display`, `Keyboard`, and `Mouse`, draw the same deterministic frame, and exit on Escape/window close.

## Requirements

Install:

- C-BuildSystem: https://xt9y.de/c.html
- RendererCheck: https://xt9y.de/rendercheck.html
- GLFW 3 development files
- OpenGL development files
- GLU development files

On Debian/Ubuntu:

```sh
sudo apt install pkg-config libglfw3-dev libgl1-mesa-dev libglu1-mesa-dev
```

`build.c` fetches:

- `https://github.com/xt9y/lwcgl.git` at `v2.9.3`
- `https://github.com/xt9y/RendererCheck.git` at `main`

Because `lwcgl` is private, your normal Git credentials must be able to clone it over HTTPS.

## Build

Build the C example:

```sh
c build crapgame-c
```

Build the C++ example:

```sh
c build crapgame-cpp
```

C-BuildSystem writes them to:

```text
build/debug/crapgame-c
build/debug/crapgame-cpp
```

The C target is the default, so this also works:

```sh
c build
c run
```

## Run

C:

```sh
./build/debug/crapgame-c
```

C++:

```sh
./build/debug/crapgame-cpp
```

## RendererCheck

Build both binaries first:

```sh
c build crapgame-c
c build crapgame-cpp
```

The first time, create the two visual baselines:

```sh
renderercheck approve c
renderercheck approve cpp
```

Then run both regression tests:

```sh
renderercheck run
```

Or run only one:

```sh
renderercheck run c
renderercheck run cpp
```

Approved images are stored under `rendercheck/baselines/`. Commit those baselines after approval if you want future renders compared against them.

## C vs C++

The C version uses the native C-compatible `DisplayMode(...)` compound-literal form:

```c
DisplayMode mode = DisplayMode(640, 360);
Display.setDisplayMode(&mode);
```

The C++ version demonstrates the LWJGL-2-shaped syntax supported by the same header:

```cpp
DisplayMode *mode = new DisplayMode(640, 360);
Display.setDisplayMode(mode);
delete mode;
```

There is no separate C++ wrapper or second graphics abstraction.
