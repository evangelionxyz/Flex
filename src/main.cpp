#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cassert>
#include <filesystem>
#include <format>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "shader.h"

struct WindowData
{
	GLFWwindow *window;
	int width, height;
};

int main()
{
	if (!glfwInit())
	{
		std::cerr << "Failed to initlaize GLFW\n";
		return 1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	WindowData windowData;
	windowData.width = 800;
	windowData.height = 400;
	
	windowData.window = glfwCreateWindow(windowData.width, windowData.height,
		"Hello OpenGL", nullptr, nullptr);
	
	if (!windowData.window)
	{
		std::cerr << "Failed to create window!\n";
		return 1;
	}

	glfwSwapInterval(1); // 1: enable vertical sync
	glfwMakeContextCurrent(windowData.window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	glfwSetWindowUserPointer(windowData.window, &windowData);
	glfwSetFramebufferSizeCallback(windowData.window, [](GLFWwindow* window, int width, int height)
	{
		WindowData &data = *static_cast<WindowData *>(glfwGetWindowUserPointer(window));
		data.width = width;
		data.height = height;

		glViewport(0, 0, width, height);

		std::cout << "Resizing framebuffer: " << width << " x " << height << "\n";
	});

	Shader shader = Shader()
		.AddFromFile("resources/shaders/default.vertex.glsl", GL_VERTEX_SHADER)
		.AddFromFile("resources/shaders/default.frag.glsl", GL_FRAGMENT_SHADER)
		.Compile();

	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glViewport(0, 0, windowData.width, windowData.height);

	double currentTime = 0.0;
	double prevTime = 0.0;
	double deltaTime = 0.0;
	double FPS = 0.0;

	double statusUpdateInterval = 0.0;
	std::string statInfo = std::format("FPS {:.3} - {:.4} s", FPS, deltaTime * 1000.0);

	while (!glfwWindowShouldClose(windowData.window))
	{
		currentTime = glfwGetTime();
		deltaTime = currentTime - prevTime;

		prevTime = currentTime;

		FPS = 1.0 / deltaTime;

		statusUpdateInterval -= deltaTime;
		if (statusUpdateInterval <= 0.0)
		{
			statInfo = std::format("OpenGL - FPS {:.3} - {:.4} ms", FPS, deltaTime * 1000.0);
			glfwSetWindowTitle(windowData.window, statInfo.c_str());
			statusUpdateInterval = 1.0;
		}

		glfwPollEvents();
		glClear(GL_COLOR_BUFFER_BIT);
		glClearColor(0.3f, 0.3f, 0.3f, 1.0f);

		shader.Use();

		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, 3);

		glfwSwapBuffers(windowData.window);
	}

	glfwDestroyWindow(windowData.window);
	glfwTerminate();

	return 0;
}
