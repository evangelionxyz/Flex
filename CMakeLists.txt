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

# Set Jolt configuration options BEFORE adding the subdirectory
set(FLOATING_POINT_EXCEPTIONS_ENABLED ON CACHE BOOL "Enable floating point exceptions" FORCE)
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE ON CACHE BOOL "Enable debug renderer" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE ON CACHE BOOL "Enable profiler" FORCE)

add_subdirectory("${THIRDPARTY_DIR}/jolt/Build")

add_subdirectory(Flex)
add_subdirectory(Tests)