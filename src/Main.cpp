#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cassert>
#include <filesystem>
#include <format>
#include <memory>

#include "Window.h"
#include "Renderer.h"
#include "Shader.h"
#include "VertexArray.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"

int main()
{
	Window window("Hello OpenGL", 800, 650);
	window.SetResizeCallback([](int width, int height)
	{
		glViewport(0, 0, width, height);
	});

	Shader shader = Shader()
		.AddFromFile("resources/shaders/default.vertex.glsl", GL_VERTEX_SHADER)
		.AddFromFile("resources/shaders/default.frag.glsl", GL_FRAGMENT_SHADER)
		.Compile();

	
	struct Vertex
	{
		float position[3];
		float color[3];
		float texCoord[2];
	};

	Vertex vertices[] = 
	{
		{ {-0.5f, -0.5f, 0.0f }, { 1.0f, 0.5, 0.0f }, {0.0f, 0.0f} }, // bottom left
		{ { 0.5f, -0.5f, 0.0f }, { 1.0f, 0.5, 0.0f }, {1.0f, 0.0f} }, // top left
		{ { 0.5f,  0.5f, 0.0f }, { 1.0f, 0.5, 0.0f }, {1.0f, 1.0f} }, // top right 
		{ {-0.5f,  0.5f, 0.0f }, { 1.0f, 0.5, 0.0f }, {0.0f, 1.0f} }, // bottom right
	};

	uint32_t indices[]
	{
		0, 1, 2,
		2, 3, 0
	};

	std::shared_ptr<VertexArray> vertexArray = std::make_shared<VertexArray>();
	std::shared_ptr<IndexBuffer> indexBuffer = std::make_shared<IndexBuffer>(indices, sizeof(indices), std::size(indices));
	std::shared_ptr<VertexBuffer> vertexBuffer = std::make_shared<VertexBuffer>(sizeof(vertices));

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void *)offsetof(Vertex, position));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void *)offsetof(Vertex, color));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void *)offsetof(Vertex, texCoord));
	glEnableVertexAttribArray(2);

	vertexArray->SetVertexBuffer(vertexBuffer);
	vertexArray->SetIndexBuffer(indexBuffer);

	double currentTime = 0.0;
	double prevTime = 0.0;
	double deltaTime = 0.0;
	double FPS = 0.0;

	double statusUpdateInterval = 0.0;
	std::string statInfo = std::format("FPS {:.3} - {:.4} s", FPS, deltaTime * 1000.0);

	while (window.IsLooping())
	{
		currentTime = glfwGetTime();
		deltaTime = currentTime - prevTime;

		prevTime = currentTime;

		FPS = 1.0 / deltaTime;

		statusUpdateInterval -= deltaTime;
		if (statusUpdateInterval <= 0.0)
		{
			statInfo = std::format("OpenGL - FPS {:.3} - {:.4} ms", FPS, deltaTime * 1000.0);
			window.SetWindowTitle(statInfo);
			statusUpdateInterval = 1.0;
		}

		glClear(GL_COLOR_BUFFER_BIT);
		glClearColor(0.3f, 0.3f, 0.3f, 1.0f);

		shader.Use();

		for (int i = 0; i < 4; ++i)
		{
			vertices[i].position[0] += 0.1f * deltaTime;
		}
		vertexBuffer->SetData(vertices, sizeof(vertices));

		Renderer::DrawIndexed(vertexArray);

		window.SwapBuffers();
	}

	return 0;
}
