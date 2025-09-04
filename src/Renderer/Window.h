#pragma once
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <string>
#include <functional>

struct WindowData
{
    std::string title;
    int width;
    int height;

    std::function<void(int width, int height)> resizeCb;
    std::function<void(int xOffset, int yOffset)> scrollCb;
};

class Window
{
public:
    Window(const std::string &title, int width, int height, bool maximized = false);
    ~Window();

    void SwapBuffers();
    bool IsLooping() const;
    void SetWindowTitle(const std::string &title);
    void SetResizeCallback(const std::function<void(int, int)> &resizeCb);
    void SetScrollCallback(const std::function<void(int, int)> &scrollCb);

    GLFWwindow *GetHandle() 
    {
        return m_Handle;
    }

private:
    GLFWwindow *m_Handle;
    WindowData m_Data;
};