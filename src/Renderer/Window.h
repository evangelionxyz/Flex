#pragma once
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <string>
#include <functional>

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

    void SwapBuffers();
    bool IsLooping() const;
    void SetWindowTitle(const std::string &title);
    void ToggleFullScreen();
    void SetKeyboardCallback(const std::function<void(int, int, int, int)> &keyCallback);
    void SetResizeCallback(const std::function<void(int, int)> &resizeCb);
    void SetScrollCallback(const std::function<void(int, int)> &scrollCb);
    void SetFullscreenCallback(const std::function<void(int, int, bool)> &fullscreenCb);
    void SetDropCallback(const std::function<void(const std::vector<std::string> &)> &dropCb) ;

    void Show();

    uint32_t GetWidth() { return m_Data.width; }
    uint32_t GetHeight() { return m_Data.height; }

    static Window *Get();

    GLFWwindow *GetHandle() 
    {
        return m_Handle;
    }

private:
    GLFWwindow *m_Handle;
    WindowData m_Data;
};