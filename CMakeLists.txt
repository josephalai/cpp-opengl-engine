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
# Source file collection
# =============================================================================

# Gather all engine source files automatically.
# Note: re-run cmake when adding/removing files (GLOB_RECURSE does not auto-detect).
file(GLOB_RECURSE ALL_SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.h"
)

# --- Separate the server entry point from the client/engine sources ---
set(SERVER_MAIN "${CMAKE_SOURCE_DIR}/src/Server/ServerMain.cpp")
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

# GLM — header-only; just add the include path, no add_subdirectory needed.
target_include_directories(client_engine PRIVATE deps/glm)

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
# Headless Server executable (NO OpenGL, NO GLFW — only ENet + GLM + stb_image)
# =============================================================================

set(SERVER_SOURCES
    ${CMAKE_SOURCE_DIR}/src/Server/ServerMain.cpp
    ${CMAKE_SOURCE_DIR}/src/Server/ServerNPCManager.cpp
    ${CMAKE_SOURCE_DIR}/src/Server/ServerStbImage.cpp
    ${CMAKE_SOURCE_DIR}/src/Terrain/HeightMap.cpp
    ${CMAKE_SOURCE_DIR}/src/Util/FileSystem.cpp
    ${CMAKE_SOURCE_DIR}/src/Network/SharedMovement.cpp
)

add_executable(headless_server ${SERVER_SOURCES})

target_include_directories(headless_server PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/deps/glm
    ${enet_SOURCE_DIR}/include
    ${nlohmann_json_SOURCE_DIR}/include
    ${entt_SOURCE_DIR}/src
)

target_compile_definitions(headless_server PRIVATE RESOURCE_ROOT="${CMAKE_SOURCE_DIR}")

target_link_libraries(headless_server PRIVATE enet)