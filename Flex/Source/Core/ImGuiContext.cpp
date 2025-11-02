// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "ImGuiContext.h"

#include "Renderer/Window.h"

namespace flex
{
    ImGuiContext::ImGuiContext(Window *window)
        : m_Window(window)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsDark();
        ImGui_ImplSDL3_InitForOpenGL(window->GetHandle(), window->GetGLContext());
        ImGui_ImplOpenGL3_Init("#version 460");
    }

    void ImGuiContext::PollEvents(SDL_Event* event)
    {
        ImGui_ImplSDL3_ProcessEvent(event);
    }

    void ImGuiContext::Shutdown()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
    
    void ImGuiContext::NewFrame()
    {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
    }
    
    void ImGuiContext::Render()
    {
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
}