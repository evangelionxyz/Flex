cmake_minimum_required(VERSION 3.21)

project(OpenGL)

set(CMAKE_CXX_STANDARD 23)

add_subdirectory("thirdparty/glfw")
add_subdirectory("thirdparty/freetype")

add_executable(opengl_app
    "src/Main.cpp"
    "src/Shader.cpp"
    "src/Window.cpp"
    "src/VertexBuffer.cpp"
    "src/VertexArray.cpp"
    "src/IndexBuffer.cpp"
    "src/Renderer.cpp"
    "src/Texture.cpp"
    "src/Font.cpp"
    "src/Pipeline.cpp"
    "thirdparty/stb/stb.c"
    
    "thirdparty/glad/src/glad.c"
)

target_include_directories(opengl_app PRIVATE
    "thirdparty/glfw/include"
    "thirdparty/glad/include"
    "thirdparty/stb"
    "thirdparty/glm"
    "thirdparty/freetype/include"
)

target_link_libraries(opengl_app PRIVATE glfw freetype)

add_custom_command(TARGET opengl_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E
    copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:opengl_app>/resources
)

