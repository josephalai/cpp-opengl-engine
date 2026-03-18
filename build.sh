#!/bin/bash

# Exit immediately if a command exits with a non-zero status.
set -e

# --- Configuration ---
# Set the absolute path to your project's root directory.
PROJECT_DIR="$HOME/Projects/cpp-game-engine"
BUILD_DIR="$PROJECT_DIR/cmake-build-debug"

# Determine the number of CPU cores for portability (macOS/Linux).
if [[ "$(uname)" == "Darwin" ]]; then
    NCPU=$(sysctl -n hw.ncpu)
elif [[ "$(uname)" == "Linux" ]]; then
    NCPU=$(nproc)
else
    # Fallback for other systems
    echo "Unrecognized OS, defaulting to 1 core for make."
    NCPU=1
fi

# --- Functions ---

# Displays usage information and exits.
usage() {
    echo "Game Engine Build & Run Script"
    echo "------------------------------"
    echo "Usage: $0 [-c] [-d] [-r] [-b] [-s] [-p] [-a] [-h]"
    echo "  -c: Clean and compile (standard build, system Assimp, no Draco)."
    echo "  -d: Clean and compile WITH Draco mesh-compression support."
    echo "      Builds Assimp from source via FetchContent. First run takes ~3-5 min."
    echo "      Use this when your GLB skins were exported with Draco compression."
    echo "  -r: Re-compile and run. Runs 'make' and then the executables."
    echo "  -b: Bake."
    echo "  -s: Server."
    echo "  -p: Bake and Run Server."
    echo "  -a: All. Performs a clean compile and then runs the application."
    echo "  -h: Display this help message."
    exit 1
}

# Performs a clean build: removes the build directory, runs cmake, then make.
clean_and_compile() {
    echo ">>> Performing a clean compile..."
    cd "$PROJECT_DIR"
    echo "--> Removing old build directory: $BUILD_DIR"
    rm -rf "$BUILD_DIR"
    echo "--> Creating new build directory and changing into it."
    mkdir -p "$BUILD_DIR" && cd "$BUILD_DIR"
    echo "--> Running CMake..."
    cmake ..
    echo "--> Compiling with make using $NCPU cores..."
    make -j"$NCPU"
    echo ">>> Clean compile finished successfully."
}

# Performs a clean build with Draco mesh-compression support.
# Builds Assimp from source (via FetchContent) so that Draco-compressed
# GLB skins (e.g. produced by AssetForge.py) can be loaded at runtime.
# On macOS with Homebrew, ZLIB_ROOT is resolved automatically so that
# Assimp uses the same system zlib that PNG and other dependencies link.
clean_and_compile_draco() {
    echo ">>> Performing a clean compile WITH Draco support..."
    cd "$PROJECT_DIR"
    echo "--> Removing old build directory: $BUILD_DIR"
    rm -rf "$BUILD_DIR"
    echo "--> Creating new build directory and changing into it."
    mkdir -p "$BUILD_DIR" && cd "$BUILD_DIR"

    # Resolve Homebrew zlib path on macOS so Assimp's FetchContent build
    # finds the same zlib that libpng and other system libs already use.
    # On Linux, system zlib is in the standard path so no override needed.
    ZLIB_ARGS=""
    if [[ "$(uname)" == "Darwin" ]] && command -v brew &>/dev/null; then
        ZLIB_ROOT="$(brew --prefix zlib)"
        echo "--> macOS detected: using Homebrew zlib at $ZLIB_ROOT"
        ZLIB_ARGS="-DZLIB_ROOT=$ZLIB_ROOT"
    fi

    echo "--> Running CMake with ENGINE_ASSIMP_WITH_DRACO=ON..."
    cmake .. -DENGINE_ASSIMP_WITH_DRACO=ON $ZLIB_ARGS
    echo "--> Compiling with make using $NCPU cores..."
    echo "    (First run will download and build Assimp + Draco from source ~3-5 min)"
    make -j"$NCPU"
    echo ">>> Clean Draco compile finished successfully."
}

bake_and_run() {
    cd "$PROJECT_DIR"
    cd "$BUILD_DIR"
    echo "--> Running asset_baker..."
    ./asset_baker
    echo "--> Running headless_server..."
    ./headless_server
}

# Re-compiles if necessary and runs the executables in sequence.
recompile_and_run() {
    echo ">>> Re-compiling and running application..."
    if [ ! -d "$BUILD_DIR" ]; then
        echo "Error: Build directory '$BUILD_DIR' not found."
        echo "Please run with the '-c', '-d', or '-a' flag first to create it."
        exit 1
    fi
    cd "$BUILD_DIR"
    echo "--> Re-compiling with make using $NCPU cores..."
    make -j"$NCPU"
    echo "--> Running asset_baker..."
    bake_and_run
}


# --- Main Logic ---

# If no arguments are provided, show usage.
if [ $# -eq 0 ]; then
    usage
fi

# Default flag states
DO_CLEAN_COMPILE=false
DO_CLEAN_COMPILE_DRACO=false
DO_RUN=false
DO_BAKE=false
DO_SERVER=false

# Parse command-line options.
while getopts "cdrahbsp" opt; do
  case $opt in
    c) DO_CLEAN_COMPILE=true ;;
    d) DO_CLEAN_COMPILE_DRACO=true ;;
    r) DO_RUN=true ;;
    b) DO_BAKE=true ;;
    s) DO_SERVER=true ;;
    p)
      DO_BAKE=true
      DO_SERVER=true
      ;;
    a)
      DO_CLEAN_COMPILE=true
      DO_SERVER=true
      ;;
    h) usage ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      usage
      ;;
  esac
done

# Execute actions based on flags.
# The order is important: compile must happen before run.
if [ "$DO_CLEAN_COMPILE_DRACO" = true ]; then
    clean_and_compile_draco
elif [ "$DO_CLEAN_COMPILE" = true ]; then
    clean_and_compile
fi

if [ "$DO_RUN" = true ]; then
    # If -a was used, clean_and_compile has already run.
    # This function will handle cd'ing into the build dir and running make + executables.
    # The second 'make' will be very fast if no files have changed.
    recompile_and_run
fi

if [ "$DO_BAKE" = true ]; then
    # If -a was used, clean_and_compile has already run.
    # This function will handle cd'ing into the build dir and running make + executables.
    # The second 'make' will be very fast if no files have changed.
    cd "$BUILD_DIR"
    ./asset_baker
fi

if [ "$DO_SERVER" = true ]; then
    # If -a was used, clean_and_compile has already run.
    # This function will handle cd'ing into the build dir and running make + executables.
    # The second 'make' will be very fast if no files have changed.
    cd "$BUILD_DIR"
    ./headless_server
fi



echo ">>> Script finished."
