cmake_minimum_required(VERSION 3.19)
project(engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Gather all source files automatically.
# Note: re-run cmake when adding/removing files (GLOB_RECURSE does not auto-detect changes).
file(GLOB_RECURSE ENGINE_SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.h"
)

add_executable(engine ${ENGINE_SOURCES})

target_include_directories(engine PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Embed the source root so FileSystem can locate resources regardless of CWD.
target_compile_definitions(engine PRIVATE RESOURCE_ROOT="${CMAKE_SOURCE_DIR}")

# --- Dependencies ---

# OpenGL — always required for this engine
find_package(OpenGL REQUIRED)
target_link_libraries(engine PRIVATE OpenGL::GL)

# GLFW
add_subdirectory(deps/glfw)
target_include_directories(engine PRIVATE deps/glfw/include)

# FreeType — use the system library (brew install freetype) to avoid
# cmake_minimum_required VERSION < 3.5 errors from the bundled source tree.
find_package(Freetype REQUIRED)

# PNG
find_package(PNG REQUIRED)
target_include_directories(engine PRIVATE ${PNG_INCLUDE_DIR})

# Assimp — use the system library (brew install assimp) for the same reason.
find_package(assimp REQUIRED)

# Quill (logging)
add_subdirectory(deps/quill)

# GLM — header-only; just add the include path, no add_subdirectory needed.
target_include_directories(engine PRIVATE deps/glm)

# Link all
target_link_libraries(engine PRIVATE
    glfw
    ${GLFW_LIBRARIES}
    Freetype::Freetype
    ${PNG_LIBRARY}
    assimp::assimp
    quill
)