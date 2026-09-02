# CrapGame

Minimal `lwcgl` **v2.9.3** C++ starter showing the LWJGL-2-style C ABI from C++.

## Files

- `main.cpp` — C++17 example using `<lwcgl/lwcgl.h>` C ABI (`new DisplayMode(...)` syntax)
- `build.c` — C-BuildSystem configuration (single `crapgame` target)
- `rendercheck.toml` — RendererCheck test for the executable

The example creates a 640x360 OpenGL 2.1 window, initializes `Display`, `Keyboard`, and `Mouse`, draws a deterministic frame, and exits on Escape/window close.

## Requirements

Install:

- C-BuildSystem: https://xt9y.de/c.html
- RendererCheck: https://xt9y.de/rendercheck.html
- `lwcgl` **v2.9.3** installed to the system (provides `/usr/local/include/lwcgl/lwcgl.h` and `/usr/local/lib/liblwcgl.a`)
- GLFW 3 development files
- OpenGL development files
- GLU development files

On Debian/Ubuntu:

```sh
sudo apt install pkg-config libglfw3-dev libgl1-mesa-dev libglu1-mesa-dev
```

Install `lwcgl` from its checkout (e.g. `../lwcgl` or `~/Documents/lwcgl`):

```sh
cd ../lwcgl   # or wherever the lwcgl checkout lives
make
sudo make install   # installs to /usr/local by default (PREFIX=/usr/local)
```

`build.c` links against the system-installed `lwcgl` (`-llwcgl`). It still fetches:

- `https://github.com/xt9y/RendererCheck.git` at `main` (header-only)

## Build

```sh
c build
```

C-BuildSystem writes the binary to:

```text
build/debug/crapgame
```

Shorthand:

```sh
c build
c run
```

## Run

```sh
./build/debug/crapgame
```

## RendererCheck

Build the binary first:

```sh
c build
```

The first time, create the visual baseline:

```sh
renderercheck approve cpp
```

Then run the regression test:

```sh
renderercheck run
```

Or explicitly:

```sh
renderercheck run cpp
```

Approved images are stored under `rendercheck/baselines/`. Commit those baselines after approval if you want future renders compared against them.

## C++ DisplayMode syntax

The C++ version demonstrates the LWJGL-2-shaped syntax supported by the same C header:

```cpp
DisplayMode *mode = new DisplayMode(640, 360);
Display.setDisplayMode(mode);
delete mode;
```

There is no separate C++ wrapper or second graphics abstraction.
