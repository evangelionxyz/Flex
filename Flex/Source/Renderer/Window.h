// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef WINDOW_H
#define WINDOW_H

#include <string>
#include <functional>
#include <unordered_map>

#include <SDL3/SDL.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_events.h>

#include <glm/glm.hpp>

namespace flex
{
    struct WindowData
    {
        std::string title;
        int x;
        int y;
        int width;
        int height;

        bool initialFullscreen = false;
        bool fullscreen = false;
        bool maximize = false;

        std::function<void(int width, int height)> resizeCb;
        std::function<void(int xOffset, int yOffset)> scrollCb;
        std::function<void(int width, int height, bool fullscreen)> fullscreenCb;
        std::function<void(int key, int scancode, int action, int mods)> keyCb;
        std::function<void(const std::vector<std::string> &paths)> dropCb;
    };

    struct WindowCreateInfo
    {
        std::string title;
        int width;
        int height;
        bool fullscreen = false;
        bool maximize = false;
    };

    class Window
    {
    public:
        Window(const WindowCreateInfo &createInfo);
        ~Window();

        void PollEvents(SDL_Event *event);

        void SwapBuffers();
        bool IsLooping() const;
        void SetWindowTitle(const std::string &title);
        
        void ToggleFullScreen();
        void Maximize() const;
        void Minimize() const;
        void Restore() const;

        void SetKeyboardCallback(const std::function<void(int, int, int, int)> &keyCallback);
        void SetResizeCallback(const std::function<void(int, int)> &resizeCb);
        void SetScrollCallback(const std::function<void(int, int)> &scrollCb);
        void SetFullscreenCallback(const std::function<void(int, int, bool)> &fullscreenCb);
        void SetDropCallback(const std::function<void(const std::vector<std::string> &)> &dropCb) ;

        void Show();

        bool IsKeyPressed(SDL_Keycode keycode);
        bool IsKeyModPressed(SDL_Keymod mod);
        bool IsMouseButtonPressed(uint32_t button);

        uint32_t GetWidth() const { return m_Data.width; }
        uint32_t GetHeight() const { return m_Data.height; }
        glm::vec2 GetMousePosition() const { return m_MousePosition; }

        static Window *Get();

        SDL_Window *GetHandle() { return m_Handle; }
        SDL_GLContext GetGLContext() const { return m_GL; }
    private:
        SDL_Window *m_Handle;
        SDL_GLContext m_GL;
        WindowData m_Data;
        bool m_Running = true;

        std::unordered_map<SDL_Keymod, bool> m_ModifierStates;
        std::unordered_map<SDL_Keycode, bool> m_KeyCodeStates;
        std::unordered_map<uint32_t, bool> m_MouseButtonStates;
        glm::vec2 m_MousePosition;
    };
}

#endif