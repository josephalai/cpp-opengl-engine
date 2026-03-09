cmake_minimum_required(VERSION 3.19)
project(engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================================
# ENet — Reliable UDP library (fetched automatically via FetchContent)
# =============================================================================
include(FetchContent)
FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG        v1.3.18
)
FetchContent_MakeAvailable(enet)

# =============================================================================
# nlohmann/json — header-only JSON library (fetched automatically)
# =============================================================================
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# =============================================================================
# EnTT — header-only ECS library (fetched automatically via FetchContent)
# =============================================================================
FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG        v3.14.0
)
FetchContent_MakeAvailable(entt)

# =============================================================================
# GLM — header-only math library (fetched automatically via FetchContent)
# Replaces the manually-cloned deps/glm so the headless server builds on any
# machine (Ubuntu VPS, CI) without requiring a local deps/ clone.
# =============================================================================
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# =============================================================================
# Source file collection — Three-way split (GEA Build Wall)
# =============================================================================
#
#  COMMON_SOURCES  Pure simulation core: ECS, Physics, Math, World, Network.
#                  Compiled into BOTH client_engine and headless_server.
#                  Must NEVER directly include OpenGL, GLFW, or GUI headers.
#
#  CLIENT_SOURCES  Everything else (rendering pipeline, GUI, animation, etc.).
#                  Compiled only into client_engine.
#
#  SERVER_SOURCES  Headless server entry point.
#                  Compiled only into headless_server.
#
# Note: re-run cmake when adding/removing files (GLOB_RECURSE does not auto-detect).

# --- COMMON: simulation core, safe for server AND client ---
file(GLOB_RECURSE COMMON_SOURCES
    "${CMAKE_SOURCE_DIR}/src/ECS/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Physics/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Toolbox/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/World/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Network/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Util/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Collision/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Config/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Terrain/HeightMap.cpp"
)

# Exclude OpenGL/GLFW-dependent files from COMMON so headless_server
# compiles cleanly without a GPU or display library.
list(FILTER COMMON_SOURCES EXCLUDE REGEX ".*/PhysicsDebugDrawer\\.cpp$")
list(FILTER COMMON_SOURCES EXCLUDE REGEX ".*/DebugLineShader\\.cpp$")
list(FILTER COMMON_SOURCES EXCLUDE REGEX ".*/CharacterController\\.cpp$")
list(FILTER COMMON_SOURCES EXCLUDE REGEX ".*/Picker\\.cpp$")
list(FILTER COMMON_SOURCES EXCLUDE REGEX ".*/MousePicker\\.cpp$")
list(FILTER COMMON_SOURCES EXCLUDE REGEX ".*/TerrainPicker\\.cpp$")
list(FILTER COMMON_SOURCES EXCLUDE REGEX ".*/MockServer\\.cpp$")

# --- SERVER: headless entry point and NPC support ---
file(GLOB_RECURSE SERVER_SOURCES
    "${CMAKE_SOURCE_DIR}/src/Server/*.cpp"
)

# --- CLIENT: full engine source tree (rendering pipeline + COMMON dirs) ---
# Uses the broad ALL_SOURCES glob so every engine subsystem is included.
file(GLOB_RECURSE ALL_SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.h"
)
set(CLIENT_SOURCES ${ALL_SOURCES})
list(FILTER CLIENT_SOURCES EXCLUDE REGEX ".*/Server/.*")

# =============================================================================
# Client Engine executable (OpenGL + GLFW + full rendering pipeline)
# =============================================================================

# The client requires OpenGL; skip if not available (e.g. headless CI).
find_package(OpenGL QUIET)
if(OpenGL_FOUND)

add_executable(client_engine ${CLIENT_SOURCES})

target_include_directories(client_engine PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Embed the source root so FileSystem can locate resources regardless of CWD.
target_compile_definitions(client_engine PRIVATE RESOURCE_ROOT="${CMAKE_SOURCE_DIR}")

# --- Client Dependencies ---

target_link_libraries(client_engine PRIVATE OpenGL::GL)

# GLFW
add_subdirectory(deps/glfw)
target_include_directories(client_engine PRIVATE deps/glfw/include)

# FreeType — use the system library (brew install freetype) to avoid
# cmake_minimum_required VERSION < 3.5 errors from the bundled source tree.
find_package(Freetype REQUIRED)

# PNG
find_package(PNG REQUIRED)
target_include_directories(client_engine PRIVATE ${PNG_INCLUDE_DIR})

# Assimp — use the system library (brew install assimp) for the same reason.
find_package(assimp REQUIRED)

# Quill (logging)
add_subdirectory(deps/quill)

# GLM — header-only; fetched via FetchContent (glm_SOURCE_DIR set above).
# deps/glm is kept as a fallback for local macOS builds with a manual clone.
target_include_directories(client_engine PRIVATE ${glm_SOURCE_DIR})

# Bullet Physics — system dependency (brew install bullet)
find_package(Bullet REQUIRED)
target_include_directories(client_engine PRIVATE ${BULLET_INCLUDE_DIRS})

# ENet include path (FetchContent populates enet_SOURCE_DIR)
target_include_directories(client_engine PRIVATE ${enet_SOURCE_DIR}/include)

# nlohmann/json — header-only (FetchContent populates nlohmann_json_SOURCE_DIR)
target_include_directories(client_engine PRIVATE ${nlohmann_json_SOURCE_DIR}/include)

# EnTT — header-only ECS library
target_include_directories(client_engine PRIVATE ${entt_SOURCE_DIR}/src)

# Link all client dependencies
target_link_libraries(client_engine PRIVATE
    glfw
    ${GLFW_LIBRARIES}
    Freetype::Freetype
    ${PNG_LIBRARY}
    assimp::assimp
    quill
    BulletDynamics
    BulletCollision
    LinearMath
    enet
)

else()
    message(STATUS "OpenGL not found — skipping client_engine target (server-only build).")
endif()

# =============================================================================
# Bullet Physics — also required by headless_server (PhysicsSystem uses Bullet).
# find_package is called outside the OpenGL block so the server target can
# link against it regardless of whether OpenGL/GLFW is present.
# =============================================================================
if(NOT Bullet_FOUND)
    find_package(Bullet QUIET)
endif()

if(NOT Bullet_FOUND)
    message(STATUS "Bullet not found via find_package — attempting FetchContent build.")
    FetchContent_Declare(
        bullet3
        GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
        GIT_TAG        3.25
    )
    set(BUILD_BULLET2_DEMOS      OFF CACHE BOOL "" FORCE)
    set(BUILD_CPU_DEMOS          OFF CACHE BOOL "" FORCE)
    set(BUILD_OPENGL3_DEMOS      OFF CACHE BOOL "" FORCE)
    set(BUILD_UNIT_TESTS         OFF CACHE BOOL "" FORCE)
    set(BUILD_EXTRAS             OFF CACHE BOOL "" FORCE)
    set(USE_GRAPHICAL_BENCHMARK  OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(bullet3)
    set(BULLET_INCLUDE_DIRS
        ${bullet3_SOURCE_DIR}/src
        ${bullet3_SOURCE_DIR}/src/BulletDynamics
        ${bullet3_SOURCE_DIR}/src/BulletCollision
        ${bullet3_SOURCE_DIR}/src/LinearMath)
    set(BULLET_LIBRARIES BulletDynamics BulletCollision LinearMath)
else()
    set(BULLET_LIBRARIES BulletDynamics BulletCollision LinearMath)
endif()

# =============================================================================
# Headless Server executable (NO OpenGL, NO GLFW — ENet + Bullet + EnTT + GLM)
# =============================================================================

add_executable(headless_server ${COMMON_SOURCES} ${SERVER_SOURCES})

target_include_directories(headless_server PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${glm_SOURCE_DIR}
    ${enet_SOURCE_DIR}/include
    ${nlohmann_json_SOURCE_DIR}/include
    ${entt_SOURCE_DIR}/src
    ${BULLET_INCLUDE_DIRS}
)

target_compile_definitions(headless_server PRIVATE
    RESOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    HEADLESS_SERVER          # Build Wall: disables OpenGL/GLFW code paths in shared headers
)

target_link_libraries(headless_server PRIVATE
    enet
    ${BULLET_LIBRARIES}
)