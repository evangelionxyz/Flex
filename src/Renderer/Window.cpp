#include "Window.h"
#include <glad/glad.h>
#include <iostream>
#include <assert.h>

#ifdef _WIN32

#ifndef GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#include <GLFW/glfw3native.h>
#include <dwmapi.h>
#include <ShellScalingApi.h>

#pragma comment(lib, "Dwmapi.lib") // Link to DWM API
#pragma comment(lib, "shcore.lib")

#endif // _WIN32

void GLFWErrorCallback(int error_code, const char* description)
{
    std::cerr << "GLFW Error: " << description << " [" << error_code << "]\n";
    assert(false);
}

Window::Window(const WindowCreateInfo &createInfo)
{
    // Set error callback as early as possible
    glfwSetErrorCallback(GLFWErrorCallback);

    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        assert(false);
    }
    
    // Request at least OpenGL 4.6 core for compute shaders
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);

	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    // glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE); // optional
    
    m_Handle = glfwCreateWindow(createInfo.width, createInfo.height, createInfo.title.c_str(), nullptr, nullptr);
    if (!m_Handle)
    {
        std::cerr << "Failed to create Window\n";
        assert(false);
    }

    m_Data.width = createInfo.width;
    m_Data.height = createInfo.height;
    m_Data.title = createInfo.title;
    m_Data.initialFullscreen = createInfo.fullscreen;
    m_Data.maximize = createInfo.maximize;

	glfwMakeContextCurrent(m_Handle);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to load GL Proc Address\n";
        assert(false);
    }

    std::cout << "GL Vendor:  " << glGetString(GL_VENDOR) << '\n';
    std::cout << "GL Renderer:" << glGetString(GL_RENDERER) << '\n';
    std::cout << "GL Version: " << glGetString(GL_VERSION) << '\n';
    
    glfwSwapInterval(1); // 1: enable vertical sync
	glfwSetWindowUserPointer(m_Handle, &m_Data);

#if _WIN32
        HWND hwnd = glfwGetWin32Window(m_Handle);
        BOOL useDarkMode = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDarkMode, sizeof(useDarkMode));

        // 7160E8 visual studio purple
        COLORREF rgbRed = 0x00E86071;
        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &rgbRed, sizeof(rgbRed));

        // DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_ROUNDSMALL;
        // DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPreference, sizeof(cornerPreference));
#endif

    if (createInfo.fullscreen)
    {
        m_Data.x = 0;
        m_Data.y = 0;

        glfwMaximizeWindow(m_Handle);
    }
    else
    {
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        int monitorWidth, monitorHeight;
        glfwGetMonitorWorkarea(monitor, nullptr, nullptr, &monitorWidth, &monitorHeight);
    
        // Center window
        m_Data.x = monitorWidth / 2 - m_Data.width / 2;
        m_Data.y = monitorHeight / 2 - m_Data.height / 2;
        glfwSetWindowPos(m_Handle, m_Data.x, m_Data.y);
    }
}

Window::~Window()
{
    glfwDestroyWindow(m_Handle);
    glfwTerminate();
}

void Window::SwapBuffers()
{
    glfwSwapBuffers(m_Handle);
}

bool Window::IsLooping() const
{
    bool shouldClose = glfwWindowShouldClose(m_Handle) == 0;
	glfwPollEvents();
    return shouldClose;
}

void Window::SetWindowTitle(const std::string &title)
{
    glfwSetWindowTitle(m_Handle, title.c_str());
}

void Window::SetResizeCallback(const std::function<void(int, int)> &resizeCb)
{
    m_Data.resizeCb = resizeCb;
    glfwSetFramebufferSizeCallback(m_Handle, [](GLFWwindow* window, int width, int height)
	{
		WindowData &data = *static_cast<WindowData *>(glfwGetWindowUserPointer(window));
		data.width = width;
		data.height = height;

        if (data.resizeCb)
            data.resizeCb(width, height);
	});
}

void Window::SetScrollCallback(const std::function<void(int, int)> &scrollCb)
{
    m_Data.scrollCb = scrollCb;
    glfwSetScrollCallback(m_Handle, [](GLFWwindow *window, double xOffset, double yOffset)
    {
        WindowData &data = *static_cast<WindowData *>(glfwGetWindowUserPointer(window));
        if (data.scrollCb)
            data.scrollCb((int)xOffset, (int)yOffset);
    });
}

void Window::SetFullscreenCallback(const std::function<void(int, int, bool)> &fullscreenCb)
{
    m_Data.fullscreenCb = fullscreenCb;
}

void Window::Show()
{
    glfwShowWindow(m_Handle);

    if (m_Data.initialFullscreen)
    {
        ToggleFullScreen();
    }
    else if (m_Data.maximize)
    {
        glfwMaximizeWindow(m_Handle);
    }
}

void Window::SetKeyboardCallback(const std::function<void(int, int, int, int)> &keyCallback)
{
    m_Data.keyCb = keyCallback;
    glfwSetKeyCallback(m_Handle, [](GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        WindowData &data = *static_cast<WindowData *>(glfwGetWindowUserPointer(window));
        if (data.keyCb)
            data.keyCb(key, scancode, action, mods);
    });
}

void Window::ToggleFullScreen()
{
    m_Data.fullscreen = !m_Data.fullscreen;
    GLFWmonitor *monitor = glfwGetWindowMonitor(m_Handle) ? glfwGetWindowMonitor(m_Handle) : glfwGetPrimaryMonitor();
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);

    if (m_Data.fullscreen)
    {
        // Save current window position and size before going fullscreen
        glfwGetWindowPos(m_Handle, &m_Data.x, &m_Data.y);
        glfwSetWindowSize(m_Handle, m_Data.width, m_Data.height);

        // Enter fullscreen
        glfwSetWindowMonitor(m_Handle, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        if (m_Data.fullscreenCb)
            m_Data.fullscreenCb(mode->width, mode->height, true);
    }
    else
    {
        // Exit fullscreen - restore window position and size
        glfwSetWindowMonitor(m_Handle, nullptr, m_Data.x, m_Data.y, m_Data.width, m_Data.height, 0);
        if (m_Data.fullscreenCb)
            m_Data.fullscreenCb(m_Data.width, m_Data.height, false);
    }
}
