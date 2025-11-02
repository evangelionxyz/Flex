// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef IMGUI_CONTEXT_H
#define IMGUI_CONTEXT_H

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_opengl3.h>

namespace flex
{
    class Window;
    class ImGuiContext
    {
    public:
        explicit ImGuiContext(Window *window);

        static void PollEvents(SDL_Event* event);
    
        static void NewFrame();
        static void Shutdown();
        void Render();

    private: 
        Window *m_Window;
    };
}

#endif