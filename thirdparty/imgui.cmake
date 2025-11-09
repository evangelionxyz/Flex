if (EXISTS "${CMAKE_SOURCE_DIR}/thirdparty/imgui/imgui.cpp")
    file(GLOB IMGUI_CORE_SRC
        thirdparty/imgui/imgui.cpp
        thirdparty/imgui/imgui_draw.cpp
        thirdparty/imgui/imgui_tables.cpp
        thirdparty/imgui/imgui_widgets.cpp
        thirdparty/imgui/imgui_demo.cpp
        thirdparty/imgui/backends/imgui_impl_sdl3.cpp
        thirdparty/imgui/backends/imgui_impl_opengl3.cpp
    )

    file(GLOB IMGUIZMO_SRC
        thirdparty/imguizmo/GraphEditor.cpp
        thirdparty/imguizmo/GraphEditor.h
        thirdparty/imguizmo/ImCurveEdit.cpp
        thirdparty/imguizmo/ImCurveEdit.h
        thirdparty/imguizmo/ImGradient.cpp
        thirdparty/imguizmo/ImGradient.h
        thirdparty/imguizmo/ImGuizmo.cpp
        thirdparty/imguizmo/ImGuizmo.h
        thirdparty/imguizmo/ImSequencer.cpp
        thirdparty/imguizmo/ImSequencer.h
        thirdparty/imguizmo/ImZoomSlider.h
    )
    
    add_library(imgui STATIC ${IMGUI_CORE_SRC} ${IMGUIZMO_SRC})
    target_include_directories(imgui PUBLIC
        thirdparty/imgui
        thirdparty/imgui/backends
        thirdparty/sdl3/include
        thirdparty/glad/include
    )
    target_compile_definitions(imgui PUBLIC IMGUI_IMPL_OPENGL_LOADER_GLAD)
else()
    message(WARNING "Dear ImGui submodule not found. Run 'git submodule add -b docking https://github.com/ocornut/imgui.git thirdparty/imgui' then re-configure.")
endif()