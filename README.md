
# C++ OpenGL Engine

A 3D game engine built with C++ and OpenGL 4.1 Core Profile, targeting **iMac 2020 Intel / macOS**.

## Quick Links

- [Installation Guide (iMac 2020 Intel / macOS)](INSTALL.md)
- [Project Contents](readmes/CONTENTS.md)
- [Usage](readmes/USAGE.md)

## Features

- OpenGL 4.1 Core Profile rendering (macOS max)
- Multi-texture terrain with height maps
- Assimp model loading (OBJ, FBX, etc.)
- FreeType2 font rendering
- GUI system (textures, rects, text)
- Bounding-box object picking (color-based)
- Skybox
- Framebuffer effects (reflection)
- Player + camera with terrain collision
- Threaded model loading

## Architecture

The engine entry point is `src/Engine/Engine.h` / `src/Engine/Engine.cpp`. The `Engine` class
provides a clear lifecycle: `init()` → `run()` → `shutdown()`.

`src/EngineTester/MainGameLoop.cpp` is retained for historical reference (learning chapters
1–24E) but is no longer the active entry point.

## Dependencies Overview

All dependencies live in `deps/` (gitignored — see [INSTALL.md](INSTALL.md) for setup):

| Library | Purpose |
|---------|---------|
| GLFW 3.3+ | Window / OpenGL context |
| GLM | Math (vectors, matrices) |
| Assimp 4.x | 3D model loading |
| FreeType 2 | Font rendering |
| libpng | PNG texture loading |
| Quill | Logging |

## Building

```bash
mkdir -p cmake-build-debug
cd cmake-build-debug
cmake ..
make -j$(sysctl -n hw.ncpu)
```

Run from the **project root** (resources use relative paths):

```bash
./cmake-build-debug/engine
```

See [INSTALL.md](INSTALL.md) for full setup instructions.
