# Installation Guide — iMac 2020 Intel / macOS

Tested on **iMac 2020 Intel** running macOS Monterey / Ventura / Sonoma with OpenGL 4.1 Core Profile.

---

## Prerequisites

### 1. Xcode Command Line Tools

```bash
xcode-select --install
```

### 2. Homebrew

```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

### 3. CMake, pkg-config, libpng

```bash
brew install cmake pkg-config libpng
```

---

## Clone the Repository

```bash
git clone https://github.com/josephalai/cpp-opengl-engine.git
cd cpp-opengl-engine
```

---

## Set Up Dependencies

The `deps/` folder is gitignored. You must clone each dependency manually:

```bash
mkdir -p deps
cd deps

# GLFW 3.3+ (window / OpenGL context)
git clone https://github.com/glfw/glfw.git

# GLM (math library — vectors, matrices)
git clone https://github.com/g-truc/glm.git

# Assimp 4.x (3D model loading — must use v4.1.0, not latest)
git clone --branch v4.1.0 --depth 1 https://github.com/assimp/assimp.git assimp-4

# FreeType 2 (font rendering)
git clone https://gitlab.freedesktop.org/freetype/freetype.git freetype2
cd freetype2
git checkout VER-2-11-0
cd ..

# Quill (logging)
git clone https://github.com/odygrd/quill.git

cd ..
```

---

## macOS OpenGL Notes

- The iMac 2020 Intel supports **OpenGL 4.1 Core Profile** (Apple's maximum).
- macOS deprecates OpenGL but it still works.  The `GL_SILENCE_DEPRECATION` define is already present in the codebase.
- On macOS, GLFW handles the OpenGL context — you generally do **not** need the `deps/glrequired` sentinel file.
- If you get OpenGL linking errors, create the sentinel: `touch deps/glrequired`
- The iMac 2020 Intel uses an **AMD Radeon Pro** GPU — fully supported.
- Retina (2×) scaling is handled automatically via `RETINA_NUMBER` in `DisplayManager.h`.

---

## Build

```bash
# From the project root
mkdir -p cmake-build-debug
cd cmake-build-debug
cmake ..
make -j$(sysctl -n hw.ncpu)
```

---

## Run

Run from the **project root** — resources are loaded relative to the working directory:

```bash
./cmake-build-debug/engine
```

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| `libpng not found` | `brew install libpng` then re-run cmake |
| Assimp build errors | Make sure you cloned the `v4.1.0` tag, not `main` |
| FreeType errors about harfbuzz / brotli | `brew install harfbuzz` or use `VER-2-11-0` tag |
| `No OpenGL context` | Ensure Xcode CLT is installed; check GPU support |
| Black screen / no textures | Run from project root, not from `cmake-build-debug/` |
| Retina display issues | Already handled — `RETINA_NUMBER` scales framebuffer automatically |
