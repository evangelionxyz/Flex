// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef IMGUI_CONTEXT_H
#define IMGUI_CONTEXT_H

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

struct GLFWwindow;

namespace flex
{
    class ImGuiContext
    {
    public:
        ImGuiContext(GLFWwindow *window);
    
        void NewFrame();
        void Render();
        void Shutdown();

    private: 
        GLFWwindow *m_Window;
    };
}

#endif