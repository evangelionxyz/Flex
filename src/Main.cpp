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
#include "Font.h"
#include "Texture.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera
{
	float zoom = 1.0f;
	glm::vec3 position;
	glm::mat4 projection;
	glm::mat4 view;
};

int main()
{
	const int WINDOW_WIDTH = 800;
	const int WINDOW_HEIGHT = 650;
	int fontScaleFactor = 1;

	Window window("Hello OpenGL", WINDOW_WIDTH, WINDOW_HEIGHT);
	Camera camera;
	camera.position = {0.0f, 0.0f, 0.0f};
	camera.view = glm::mat4(1.0f); // Identity matrix for 2D rendering

	// Set up orthographic projection from (0,0) to (width, height)
	camera.projection = glm::ortho(0.0f, (float)WINDOW_WIDTH, 0.0f, (float)WINDOW_HEIGHT);

	window.SetResizeCallback([&](int width, int height)
	{
		glViewport(0, 0, width, height);

		// Update projection to maintain (0,0) to (width, height) coordinate system
		camera.projection = glm::ortho(0.0f, (float)width, 0.0f, (float)height);
	});

	std::shared_ptr<Texture2D> texture = std::make_shared<Texture2D>("resources/textures/image.jpg");

	Shader shader = Shader()
		.AddFromFile("resources/shaders/default.vertex.glsl", GL_VERTEX_SHADER)
		.AddFromFile("resources/shaders/default.frag.glsl", GL_FRAGMENT_SHADER)
		.Compile();

	Shader textShader = Shader()
		.AddFromFile("resources/shaders/text.vertex.glsl", GL_VERTEX_SHADER)
		.AddFromFile("resources/shaders/text.frag.glsl", GL_FRAGMENT_SHADER)
		.Compile();

	
	struct Vertex
	{
		float position[3];
		float color[3];
		float texCoord[2];
	};

	float scale = 50.0f;
	Vertex vertices[] = 
	{
		{ { 0.0f * scale,  0.0f * scale, 0.0f }, { 1.0f, 0.5, 0.0f }, {0.0f, 0.0f} }, // bottom left
		{ { 1.0f * scale,  0.0f * scale, 0.0f }, { 1.0f, 0.5, 0.0f }, {1.0f, 0.0f} }, // top left
		{ { 1.0f * scale,  1.0f * scale, 0.0f }, { 1.0f, 0.5, 0.0f }, {1.0f, 1.0f} }, // top right 
		{ { 0.0f * scale,  1.0f * scale, 0.0f }, { 1.0f, 0.5, 0.0f }, {0.0f, 1.0f} }, // bottom right
	};

	uint32_t indices[]
	{
		0, 1, 2,
		2, 3, 0
	};

	std::shared_ptr<VertexArray> vertexArray = std::make_shared<VertexArray>();
	std::shared_ptr<IndexBuffer> indexBuffer = std::make_shared<IndexBuffer>(indices, sizeof(indices), std::size(indices));
	std::shared_ptr<VertexBuffer> vertexBuffer = std::make_shared<VertexBuffer>(vertices, sizeof(vertices));

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

	std::shared_ptr<Font> font = std::make_shared<Font>("resources/fonts/Montserrat-Regular.ttf", 16);

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

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		glClearColor(0.01f, 0.01f, 0.01f, 1.0f);

		// Text
		{
		 	glEnable(GL_BLEND);
		 	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			
		 	textShader.Use();
			textShader.SetUniform("viewProjection", camera.projection * camera.view);
		 	textShader.SetUniform("texture0", 0);
		 	float color[3] = {1.0f, 1.0f, 1.0f};

		 	// Position text from top-left corner (accounting for OpenGL's bottom-left origin)
			for (int i = 1; i <= 1; ++i)
			{
				const int yPos = WINDOW_HEIGHT - font->GetFontSize() * i * fontScaleFactor; 
				font->DrawText(std::format("Text {}", i), 5, yPos, fontScaleFactor, color);
			}
		}

		// Geometry
		{
			texture->Bind(0);

			shader.Use();
			shader.SetUniform("viewProjection", camera.projection * camera.view);
			shader.SetUniform("texture0", 0);
			Renderer::DrawIndexed(vertexArray);
		}

		window.SwapBuffers();
	}

	return 0;
}
