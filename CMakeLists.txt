cmake_minimum_required(VERSION 3.21)

project(Flex LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)

set(RESOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Resources")
set(THIRDPARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty")

add_subdirectory("${THIRDPARTY_DIR}/glfw")
include("${THIRDPARTY_DIR}/msdfgen.cmake")
include("${THIRDPARTY_DIR}/msdfatlasgen.cmake")
include("${THIRDPARTY_DIR}/freetype.cmake")
include("${THIRDPARTY_DIR}/imgui.cmake")

add_subdirectory(Flex)