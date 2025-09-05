cmake_minimum_required(VERSION 3.21)

project(OpenGL)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory("thirdparty/glfw")

include("thirdparty/msdfgen.cmake")
include("thirdparty/msdfatlasgen.cmake")
include("thirdparty/freetype.cmake")
include("thirdparty/imgui.cmake")

file(GLOB_RECURSE app_src_file
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/src/*.h
    ${CMAKE_SOURCE_DIR}/src/*.cpp
    ${CMAKE_SOURCE_DIR}/src/*.hpp
)

add_executable(opengl_app
    ${app_src_file}
    "thirdparty/tinygltf/tinygltf.cpp"
    "thirdparty/stb/stb.c"
    "thirdparty/glad/src/glad.c"
)

target_include_directories(opengl_app PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "thirdparty/glfw/include"
    "thirdparty/glad/include"
    "thirdparty/stb"
    "thirdparty/glm"
    "thirdparty/json"
    "thirdparty/tinygltf/include"
)

target_link_libraries(opengl_app PRIVATE glfw FREETYPE MSDFGEN MSDF_ATLAS_GEN imgui)

add_custom_command(TARGET opengl_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:opengl_app>/resources
)

