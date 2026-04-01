cmake_minimum_required(VERSION 3.19)
project(engine_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# --- GoogleTest ---
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# --- GLM (header-only maths library, required by animation code) ---
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

enable_testing()

# Animation sources needed by the tests (no OpenGL dependencies).
set(ANIMATION_SOURCES
    ${CMAKE_SOURCE_DIR}/../src/Animation/Skeleton.cpp
    ${CMAKE_SOURCE_DIR}/../src/Animation/Bone.cpp
    ${CMAKE_SOURCE_DIR}/../src/Animation/AnimationController.cpp
    ${CMAKE_SOURCE_DIR}/../src/Animation/AnimationBlender.cpp
    ${CMAKE_SOURCE_DIR}/../src/Animation/AnimationClip.cpp
    ${CMAKE_SOURCE_DIR}/../src/Animation/AnimationState.cpp
)

# Collect all test_*.cpp files in this directory.
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/*.cpp")

add_executable(engine_tests ${TEST_SOURCES} ${ANIMATION_SOURCES})

target_include_directories(engine_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/../src
    ${glm_SOURCE_DIR}
)

target_compile_definitions(engine_tests PRIVATE
    HEADLESS_SERVER
)

target_link_libraries(engine_tests PRIVATE
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(engine_tests)
