// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "Window.h"
#include <glad/glad.h>
#include <iostream>
#include <assert.h>

#ifdef _WIN32
    #include <dwmapi.h>
    #include <ShellScalingApi.h>

    #pragma comment(lib, "Dwmapi.lib") // Link to DWM API
    #pragma comment(lib, "shcore.lib")
#endif // _WIN32

namespace flex
{
    Window *s_Window = nullptr;
    Window::Window(const WindowCreateInfo &createInfo)
    {
        s_Window = this;

        if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_HAPTIC | SDL_INIT_JOYSTICK))
        {
            assert(false && "Failed to initialize SDL");
            exit(1);
        }
        
        m_Handle = SDL_CreateWindow(createInfo.title.c_str(), createInfo.width, createInfo.height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
        if (!m_Handle)
        {
            assert(false && "Failed to create SDL Window");
            exit(1);
        }

        m_Data.width = createInfo.width;
        m_Data.height = createInfo.height;
        m_Data.title = createInfo.title;
        m_Data.initialFullscreen = createInfo.fullscreen;
        m_Data.maximize = createInfo.maximize;

        m_GL = SDL_GL_CreateContext(m_Handle);
        if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
        {
            std::cerr << "Failed to load GL Proc Address\n";
            assert(false);
        }

        std::cout << "GL Vendor:  " << glGetString(GL_VENDOR) << '\n';
        std::cout << "GL Renderer:" << glGetString(GL_RENDERER) << '\n';
        std::cout << "GL Version: " << glGetString(GL_VERSION) << '\n';
        
        SDL_GL_SetSwapInterval(1);

#if _WIN32
        SDL_PropertiesID prop = SDL_GetWindowProperties(m_Handle);
        HWND hwnd = (HWND)SDL_GetPropertyType(prop, SDL_PROP_WINDOW_WIN32_HWND_POINTER);
        BOOL useDarkMode = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

        // 7160E8 visual studio purple
        COLORREF rgbRed = 0x00E86071;
        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));
#endif
    }

    Window::~Window()
    {
        s_Window = nullptr;

        SDL_DestroyWindow(m_Handle);
        SDL_Quit();
    }

    void Window::PollEvents(SDL_Event *event)
    {
        if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
        {
            m_Data.width = event->window.data1;
            m_Data.height = event->window.data1;

            if (m_Data.resizeCb)
            {
                m_Data.resizeCb(m_Data.width, m_Data.height);
            }
        }
        else if (event->type == SDL_EVENT_KEY_DOWN)
        {
            m_ModifierStates[SDL_KMOD_SHIFT] = event->key.mod & SDL_KMOD_SHIFT;
            m_ModifierStates[SDL_KMOD_CTRL] = event->key.mod & SDL_KMOD_CTRL;
            m_ModifierStates[SDL_KMOD_LALT] = event->key.mod & SDL_KMOD_LALT;
            m_ModifierStates[SDL_KMOD_RALT] = event->key.mod & SDL_KMOD_RALT;
            m_ModifierStates[SDL_KMOD_LSHIFT] = event->key.mod & SDL_KMOD_LSHIFT;
            m_ModifierStates[SDL_KMOD_RSHIFT] = event->key.mod & SDL_KMOD_RSHIFT;
            m_ModifierStates[SDL_KMOD_LCTRL] = event->key.mod & SDL_KMOD_LCTRL;
            m_ModifierStates[SDL_KMOD_RCTRL] = event->key.mod & SDL_KMOD_RCTRL;

            m_KeyCodeStates[event->key.key] = true;

            if (m_Data.keyCb)
            {
                // m_Data.keyCb(key, scancode, action, mods);
            }
        }
        else if (event->type == SDL_EVENT_KEY_UP)
        {
            m_ModifierStates[SDL_KMOD_SHIFT] = event->key.mod & SDL_KMOD_SHIFT;
            m_ModifierStates[SDL_KMOD_CTRL] = event->key.mod & SDL_KMOD_CTRL;
            m_ModifierStates[SDL_KMOD_LALT] = event->key.mod & SDL_KMOD_LALT;
            m_ModifierStates[SDL_KMOD_RALT] = event->key.mod & SDL_KMOD_RALT;
            m_ModifierStates[SDL_KMOD_LSHIFT] = event->key.mod & SDL_KMOD_LSHIFT;
            m_ModifierStates[SDL_KMOD_RSHIFT] = event->key.mod & SDL_KMOD_RSHIFT;
            m_ModifierStates[SDL_KMOD_LCTRL] = event->key.mod & SDL_KMOD_LCTRL;
            m_ModifierStates[SDL_KMOD_RCTRL] = event->key.mod & SDL_KMOD_RCTRL;

            m_KeyCodeStates[event->key.key] = false;
        }
        else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
        {
            m_MouseButtonStates[event->button.button] = true;
        }
        else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP)
        {
            m_MouseButtonStates[event->button.button] = false;
        }
        else if (event->type == SDL_EVENT_MOUSE_MOTION)
        {
            m_MousePosition = { static_cast<float>(event->motion.x), static_cast<float>(event->motion.y) };
        }
        else if (event->type == SDL_EVENT_MOUSE_WHEEL)
        {
            if (m_Data.scrollCb)
            {
                m_Data.scrollCb((int)event->wheel.x, (int)event->wheel.x);
            }
        }
    }

    void Window::SwapBuffers()
    {
        SDL_GL_SwapWindow(m_Handle);
    }

    bool Window::IsLooping() const
    {
        return m_Running;
    }

    void Window::SetWindowTitle(const std::string &title)
    {
        SDL_SetWindowTitle(m_Handle, title.c_str());
    }

    void Window::SetResizeCallback(const std::function<void(int, int)> &resizeCb)
    {
        m_Data.resizeCb = resizeCb;
    }

    void Window::SetScrollCallback(const std::function<void(int, int)> &scrollCb)
    {
        m_Data.scrollCb = scrollCb;
    }

    void Window::SetFullscreenCallback(const std::function<void(int, int, bool)> &fullscreenCb)
    {
        m_Data.fullscreenCb = fullscreenCb;
    }

    void Window::SetDropCallback(const std::function<void(const std::vector<std::string> &)> &dropCb)
    {
        m_Data.dropCb = dropCb;
        // glfwSetDropCallback(m_Handle, [](GLFWwindow *window, int pathCount, const char **paths)
        // {
        //     WindowData &data = *static_cast<WindowData *>(glfwGetWindowUserPointer(window));
        // 
        //     std::vector<std::string> stringPaths;
        //     for (int i = 0; i < pathCount; ++i)
        //         stringPaths.push_back(paths[i]);
        // 
        //     if (data.dropCb)
        //     {
        //         data.dropCb(stringPaths);
        //     }
        // });
    }

    void Window::Show()
    {
        SDL_ShowWindow(m_Handle);
        
        if (m_Data.initialFullscreen)
        {
            ToggleFullScreen();
        }
        else if (m_Data.maximize)
        {
            Maximize();
        }
    }

    bool Window::IsKeyPressed(SDL_Keycode keycode)
    {
        return m_KeyCodeStates[keycode];
    }

    bool Window::IsKeyModPressed(SDL_Keymod mod)
    {
        return m_ModifierStates[mod];
    }

    bool Window::IsMouseButtonPressed(uint32_t button)
    {
        return m_MouseButtonStates[button];
    }

    Window *Window::Get()
    {
        return s_Window;
    }

    void Window::SetKeyboardCallback(const std::function<void(int, int, int, int)> &keyCallback)
    {
        m_Data.keyCb = keyCallback;
    }

    void Window::ToggleFullScreen()
    {
        m_Data.fullscreen = !m_Data.fullscreen;
        SDL_SetWindowFullscreen(m_Handle, m_Data.fullscreen);
    }

    void Window::Maximize() const
    {
        SDL_MaximizeWindow(m_Handle);
    }

    void Window::Minimize() const
    {
        if (!m_Data.fullscreen)
        {
            SDL_SetWindowFullscreen(m_Handle, false);
        }

        SDL_MinimizeWindow(m_Handle);
    }

    void Window::Restore() const
    {
        SDL_RestoreWindow(m_Handle);
    }
}