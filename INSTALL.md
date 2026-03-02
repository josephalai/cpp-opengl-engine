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

### 3. CMake, pkg-config, libpng, FreeType, Assimp

```bash
brew install cmake pkg-config libpng freetype assimp
```

---

## Clone the Repository

```bash
git clone https://github.com/josephalai/cpp-opengl-engine.git
cd cpp-opengl-engine
```

---

## Set Up Dependencies

The `deps/` folder is gitignored. Clone the deps that are still built from source:

```bash
mkdir -p deps
cd deps

# GLFW 3.3+ (window / OpenGL context)
git clone https://github.com/glfw/glfw.git

# GLM (math library — vectors, matrices; header-only)
git clone https://github.com/g-truc/glm.git

# Quill (logging)
git clone https://github.com/odygrd/quill.git

cd ..
```

> **FreeType** and **Assimp** are now installed as system libraries via `brew install freetype assimp`
> (see Prerequisites above).  Do **not** clone them into `deps/` — the build uses
> `find_package` to locate the brew-installed versions.

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
| `FreeType not found` | `brew install freetype` then re-run cmake |
| `assimp not found` | `brew install assimp` then re-run cmake |
| CMake error about `cmake_minimum_required` / VERSION < 3.5 | Ensure FreeType and Assimp are installed via brew (not cloned into `deps/`) |
| `No OpenGL context` | Ensure Xcode CLT is installed; check GPU support |
| Black screen / no textures | Run from project root, not from `cmake-build-debug/` |
| Retina display issues | Already handled — `RETINA_NUMBER` scales framebuffer automatically |
