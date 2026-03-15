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
    echo "Usage: $0 [-c] [-r] [-a] [-h]"
    echo "  -c: Clean and compile. Removes the old build directory and runs CMake and make."
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
        echo "Please run with the '-c' or '-a' flag first to create it."
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
DO_RUN=false
DO_BAKE=false
DO_SERVER=false

# Parse command-line options.
while getopts "crahbsp" opt; do
  case $opt in
    c) DO_CLEAN_COMPILE=true ;;
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
if [ "$DO_CLEAN_COMPILE" = true ]; then
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
