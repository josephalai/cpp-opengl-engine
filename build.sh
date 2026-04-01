#!/bin/bash

set -e

# --- Configuration ---
PROJECT_DIR="$HOME/Projects/cpp-game-engine"
BUILD_DIR="$PROJECT_DIR/cmake-build-debug"

if [[ "$(uname)" == "Darwin" ]]; then
    NCPU=$(sysctl -n hw.ncpu)
elif [[ "$(uname)" == "Linux" ]]; then
    NCPU=$(nproc)
else
    echo "Unrecognized OS, defaulting to 1 core for make."
    NCPU=1
fi

# --- Functions ---

usage() {
    echo "Game Engine Build & Run Script"
    echo "------------------------------"
    echo "Usage: $0 [-t] [-c] [-d] [-r] [-b] [-s] [-p] [-a] [-h]"
    echo "  -t: Enable unit tests (adds ENGINE_BUILD_TESTS=ON to CMake)."
    echo "      Must be combined with -c, -d, or -a (a clean compile is required"
    echo "      to configure CMake with the test flag)."
    echo "  -c: Clean and compile (standard build, system Assimp, no Draco)."
    echo "  -d: Clean and compile WITH Draco mesh-compression support."
    echo "      Builds Assimp from source via FetchContent. First run ~3-5 min."
    echo "  -r: Re-compile and run. Runs 'make' and then the executables."
    echo "  -b: Bake."
    echo "  -s: Server."
    echo "  -p: Bake and Run Server."
    echo "  -a: All. Performs a clean compile and then runs the application."
    echo "  -h: Display this help message."
    echo ""
    echo "Examples:"
    echo "  ./build.sh -d          # Clean + Draco build, no tests"
    echo "  ./build.sh -t -d       # Clean + Draco build, WITH tests"
    echo "  ./build.sh -t -c       # Clean standard build, WITH tests"
    echo "  ./build.sh -r          # Incremental recompile + run (no CMake re-run)"
    echo "  ./build.sh -p          # Bake + start server"
    exit 1
}

clean_and_compile() {
    echo ">>> Performing a clean compile..."
    cd "$PROJECT_DIR"
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR" && cd "$BUILD_DIR"
    echo "--> Running CMake (ENGINE_BUILD_TESTS=$BUILD_TESTS_FLAG)..."
    cmake .. -DENGINE_BUILD_TESTS="$BUILD_TESTS_FLAG"
    echo "--> Compiling with make using $NCPU cores..."
    make -j"$NCPU"
    echo ">>> Clean compile finished successfully."
}

clean_and_compile_draco() {
    echo ">>> Performing a clean compile WITH Draco support..."
    cd "$PROJECT_DIR"
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR" && cd "$BUILD_DIR"

    ZLIB_ARGS=""
    if [[ "$(uname)" == "Darwin" ]] && command -v brew &>/dev/null; then
        ZLIB_ROOT="$(brew --prefix zlib)"
        echo "--> macOS detected: using Homebrew zlib at $ZLIB_ROOT"
        ZLIB_ARGS="-DZLIB_ROOT=$ZLIB_ROOT"
    fi

    echo "--> Running CMake with ENGINE_ASSIMP_WITH_DRACO=ON, ENGINE_BUILD_TESTS=$BUILD_TESTS_FLAG..."
    cmake .. -DENGINE_ASSIMP_WITH_DRACO=ON -DENGINE_BUILD_TESTS="$BUILD_TESTS_FLAG" $ZLIB_ARGS
    echo "--> Compiling with make using $NCPU cores..."
    echo "    (First run will download and build Assimp + Draco from source ~3-5 min)"
    make -j"$NCPU"
    echo ">>> Clean Draco compile finished successfully."
}

bake_and_run() {
    cd "$BUILD_DIR"
    echo "--> Running asset_baker..."
    ./asset_baker
    echo "--> Running headless_server..."
    ./headless_server
}

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
    bake_and_run
}

run_tests() {
    echo ">>> Running engine_tests..."
    if [ ! -f "$BUILD_DIR/engine_tests" ]; then
        echo "Error: engine_tests binary not found in $BUILD_DIR."
        echo "Did you forget to pass -t during your last clean compile?"
        exit 1
    fi
    cd "$BUILD_DIR"
    ctest --output-on-failure
}

# --- Main Logic ---

if [ $# -eq 0 ]; then
    usage
fi

# Default flag states
BUILD_TESTS_FLAG="OFF"   # <-- NEW: default is no tests
DO_CLEAN_COMPILE=false
DO_CLEAN_COMPILE_DRACO=false
DO_RUN=false
DO_BAKE=false
DO_SERVER=false
DO_RUN_TESTS=false

# Notice the added 't' at the beginning of the string
while getopts "tcdrahbsp" opt; do
  case $opt in
    t)
      BUILD_TESTS_FLAG="ON"   # <-- NEW
      DO_RUN_TESTS=true       # <-- NEW: also run them after build
      ;;
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

# Warn if -t is passed without a clean compile flag
if [ "$BUILD_TESTS_FLAG" = "ON" ] && [ "$DO_CLEAN_COMPILE" = false ] && [ "$DO_CLEAN_COMPILE_DRACO" = false ]; then
    echo "WARNING: -t was passed without -c, -d, or -a."
    echo "         CMake will NOT be re-run, so ENGINE_BUILD_TESTS may not take effect."
    echo "         If engine_tests doesn't exist, add -c or -d to your command."
fi

# Execute actions based on flags (compile must happen before run/test)
if [ "$DO_CLEAN_COMPILE_DRACO" = true ]; then
    clean_and_compile_draco
elif [ "$DO_CLEAN_COMPILE" = true ]; then
    clean_and_compile
fi

if [ "$DO_RUN" = true ]; then
    recompile_and_run
fi

if [ "$DO_BAKE" = true ]; then
    cd "$BUILD_DIR"
    ./asset_baker
fi

if [ "$DO_SERVER" = true ]; then
    cd "$BUILD_DIR"
    ./headless_server
fi

if [ "$DO_RUN_TESTS" = true ]; then
    run_tests
fi

echo ">>> Script finished."