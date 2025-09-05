if (EXISTS "${CMAKE_SOURCE_DIR}/thirdparty/imgui/imgui.cpp")
    file(GLOB IMGUI_CORE_SRC
        thirdparty/imgui/imgui.cpp
        thirdparty/imgui/imgui_draw.cpp
        thirdparty/imgui/imgui_tables.cpp
        thirdparty/imgui/imgui_widgets.cpp
        thirdparty/imgui/imgui_demo.cpp
        thirdparty/imgui/backends/imgui_impl_glfw.cpp
        thirdparty/imgui/backends/imgui_impl_opengl3.cpp
    )
    add_library(imgui STATIC ${IMGUI_CORE_SRC})
    target_include_directories(imgui PUBLIC
        thirdparty/imgui
        thirdparty/imgui/backends
        thirdparty/glfw/include
        thirdparty/glad/include
    )
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
    target_link_libraries(imgui PUBLIC glfw)
else()
    message(WARNING "Dear ImGui submodule not found. Run 'git submodule add -b docking https://github.com/ocornut/imgui.git thirdparty/imgui' then re-configure.")
endif()