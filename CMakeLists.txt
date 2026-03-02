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

# --- Dependencies ---

# OpenGL (conditional for laptop vs desktop)
if (EXISTS ${CMAKE_SOURCE_DIR}/deps/glrequired)
    message(STATUS "Requiring OpenGL")
    find_package(OpenGL REQUIRED)
    target_link_libraries(engine PRIVATE OpenGL::GL)
endif()

# GLFW
add_subdirectory(deps/glfw)
target_include_directories(engine PRIVATE deps/glfw/include)

# FreeType
add_subdirectory(deps/freetype2)
target_include_directories(engine PRIVATE deps/freetype2/include)

# PNG
find_package(PNG REQUIRED)
target_include_directories(engine PRIVATE ${PNG_INCLUDE_DIR})

# Assimp
add_subdirectory(deps/assimp-4)
target_include_directories(engine PRIVATE deps/assimp-4/include)

# Quill (logging)
add_subdirectory(deps/quill)

# GLM
add_subdirectory(deps/glm EXCLUDE_FROM_ALL)
target_include_directories(engine PRIVATE deps/glm)

# Link all
target_link_libraries(engine PRIVATE
    glfw
    ${GLFW_LIBRARIES}
    freetype
    ${PNG_LIBRARY}
    assimp
    quill
    glm
)