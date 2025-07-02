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
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        assert(false);
    }
    
    // GLFW v3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    m_Handle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!m_Handle)
    {
        std::cerr << "Failed to create Window\n";
        assert(false);
    }
    
    glfwSetErrorCallback(GLFWErrorCallback);

    m_Data.width = width;
    m_Data.height = height;
    m_Data.title = title;

	glfwMakeContextCurrent(m_Handle);
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to load GL Proc Address\n";
        assert(false);
    }

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

