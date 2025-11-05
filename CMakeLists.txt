cmake_minimum_required(VERSION 3.21)

project(Flex LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)

set(RESOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Flex/Resources")
set(THIRDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty")

include("${THIRDPARTY_DIR}/msdfgen.cmake")
include("${THIRDPARTY_DIR}/msdfatlasgen.cmake")
include("${THIRDPARTY_DIR}/freetype.cmake")
include("${THIRDPARTY_DIR}/imgui.cmake")


add_subdirectory("${THIRDPARTY_DIR}/sdl3")

# Configure Jolt Physics as a static library
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "Disable warnings as errors for Jolt" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build Jolt as a static library" FORCE)
set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF CACHE BOOL "Use dynamic MSVC runtime library" FORCE)
add_subdirectory("${THIRDPARTY_DIR}/jolt/Build")

# Configure Jolt with static library settings
target_compile_definitions(Jolt PRIVATE 
    JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
    JPH_DEBUG_RENDERER
    JPH_PROFILE_ENABLED
    JPH_OBJECT_STREAM
)

add_subdirectory(Flex)
add_subdirectory(Tests)