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
# =============================================================================
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# =============================================================================
# Sol2 — header-only C++↔Lua binding library (fetched automatically)
# =============================================================================
FetchContent_Declare(
    sol2
    GIT_REPOSITORY https://github.com/ThePhD/sol2.git
    GIT_TAG        main
)
FetchContent_MakeAvailable(sol2)

# =============================================================================
# Lua 5.4 — Try system package first, then FetchContent as fallback
# =============================================================================
find_package(Lua 5.4 QUIET)
if(LUA_FOUND)
    message(STATUS "Using system Lua: ${LUA_INCLUDE_DIR}")
    set(LUA_INCLUDE_DIRS "${LUA_INCLUDE_DIR}")
    set(LUA_LIBRARIES "${LUA_LIBRARIES}")
else()
    FetchContent_Declare(
        lua54
        URL https://www.lua.org/ftp/lua-5.4.6.tar.gz
    )
    FetchContent_MakeAvailable(lua54)

    # Create a static library out of the raw Lua C files
    if(NOT TARGET lua54_lib)
        file(GLOB LUA_SOURCES "${lua54_SOURCE_DIR}/src/*.c")
        # Remove the standalone executables so we only build the library
        list(REMOVE_ITEM LUA_SOURCES 
            "${lua54_SOURCE_DIR}/src/lua.c" 
            "${lua54_SOURCE_DIR}/src/luac.c"
        )
        
        add_library(lua54_lib STATIC ${LUA_SOURCES})
        target_include_directories(lua54_lib PUBLIC "${lua54_SOURCE_DIR}/src")
        
        # OS-specific macros for Lua
        if(APPLE)
            target_compile_definitions(lua54_lib PRIVATE LUA_USE_MACOSX)
        elseif(UNIX)
            target_compile_definitions(lua54_lib PRIVATE LUA_USE_LINUX)
        endif()

        # Expose variables to the rest of the CMake file
        set(LUA_FOUND TRUE)
        set(LUA_LIBRARIES lua54_lib)
        set(LUA_INCLUDE_DIRS "${lua54_SOURCE_DIR}/src")
        
        message(STATUS "Lua 5.4 source downloaded and added as a build target.")
    endif()
endif()

# =============================================================================
# Source file collection — Three-way split (GEA Build Wall)
# =============================================================================
file(GLOB_RECURSE COMMON_SOURCES
    "${CMAKE_SOURCE_DIR}/src/ECS/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Physics/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Toolbox/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Network/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Util/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Collision/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Config/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Scripting/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Terrain/HeightMap.cpp"
    "${CMAKE_SOURCE_DIR}/src/Engine/GLUploadQueue.cpp"
)

# Exclude OpenGL/GLFW-dependent files from COMMON
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
file(GLOB_RECURSE ALL_SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/*.h"
)
set(CLIENT_SOURCES ${ALL_SOURCES})
list(FILTER CLIENT_SOURCES EXCLUDE REGEX ".*/Server/.*")

# =============================================================================
# Client Engine executable (OpenGL + GLFW + full rendering pipeline)
# =============================================================================
find_package(OpenGL QUIET)
if(OpenGL_FOUND)

add_executable(client_engine ${CLIENT_SOURCES})

target_include_directories(client_engine PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(client_engine PRIVATE RESOURCE_ROOT="${CMAKE_SOURCE_DIR}")

target_link_libraries(client_engine PRIVATE OpenGL::GL)

add_subdirectory(deps/glfw)
target_include_directories(client_engine PRIVATE deps/glfw/include)

find_package(Freetype REQUIRED)
find_package(PNG REQUIRED)
target_include_directories(client_engine PRIVATE ${PNG_INCLUDE_DIR})

find_package(assimp REQUIRED)

add_subdirectory(deps/quill)

target_include_directories(client_engine PRIVATE ${glm_SOURCE_DIR})

find_package(Bullet REQUIRED)
target_include_directories(client_engine PRIVATE ${BULLET_INCLUDE_DIRS})

target_include_directories(client_engine PRIVATE ${enet_SOURCE_DIR}/include)
target_include_directories(client_engine PRIVATE ${nlohmann_json_SOURCE_DIR}/include)
target_include_directories(client_engine PRIVATE ${entt_SOURCE_DIR}/src)

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
# Bullet Physics
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
    HEADLESS_SERVER          
)

target_link_libraries(headless_server PRIVATE
    enet
    ${BULLET_LIBRARIES}
)

# =============================================================================
# Asset Baker — offline CLI tool (GEA Step 5.1)
# Reads scene.json + heightmaps, pre-calculates entity positions, and writes
# binary chunk_X_Z.dat files for instantaneous runtime loading.
# =============================================================================
add_executable(AssetBaker
    ${CMAKE_SOURCE_DIR}/src/Tools/AssetBaker.cpp
    ${CMAKE_SOURCE_DIR}/src/Tools/ToolsStbImage.cpp
    ${CMAKE_SOURCE_DIR}/src/Terrain/HeightMap.cpp
    ${CMAKE_SOURCE_DIR}/src/Util/FileSystem.cpp
)

target_include_directories(AssetBaker PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${glm_SOURCE_DIR}
    ${nlohmann_json_SOURCE_DIR}/include
)

target_compile_definitions(AssetBaker PRIVATE
    RESOURCE_ROOT="${CMAKE_SOURCE_DIR}"
    HEADLESS_SERVER
)

# =============================================================================
# Sol2 + Lua Linking (for both Server and Client)
# =============================================================================
if(LUA_FOUND)
    # 1. Link Headless Server
    target_include_directories(headless_server BEFORE PRIVATE
        ${LUA_INCLUDE_DIRS}
        ${sol2_SOURCE_DIR}/include
    )
    target_compile_definitions(headless_server PRIVATE HAS_LUA)
    target_link_libraries(headless_server PRIVATE ${LUA_LIBRARIES})

    # 2. Link Client Engine
    if(TARGET client_engine)
        target_include_directories(client_engine BEFORE PRIVATE
            ${LUA_INCLUDE_DIRS}
            ${sol2_SOURCE_DIR}/include
        )
        target_compile_definitions(client_engine PRIVATE HAS_LUA)
        target_link_libraries(client_engine PRIVATE ${LUA_LIBRARIES})
    endif()
endif()