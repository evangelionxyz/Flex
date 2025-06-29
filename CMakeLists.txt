cmake_minimum_required(VERSION 3.21)

project(OpenGL)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory("thirdparty/glfw")

add_executable(opengl_app
    "src/main.cpp"
    "src/shader.cpp"

    "thirdparty/glad/src/glad.c"
)

target_include_directories(opengl_app PRIVATE
    "thirdparty/glfw/include"
    "thirdparty/glad/include"
)
target_link_libraries(opengl_app PRIVATE glfw)

