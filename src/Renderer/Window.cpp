#include "Window.h"
#include <glad/glad.h>
#include <iostream>
#include <assert.h>

void GLFWErrorCallback(int error_code, const char* description)
{
    std::cerr << "GLFW Error: " << description << " [" << error_code << "]\n";
    assert(false);
}

Window::Window(const std::string &title, int width, int height, bool maximized)
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
    // glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE); // optional
    
    m_Handle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_Handle)
    {
        std::cerr << "Failed to create Window\n";
        assert(false);
    }

    m_Data.width = width;
    m_Data.height = height;
    m_Data.title = title;

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
