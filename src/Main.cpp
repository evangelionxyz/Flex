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

	double currentTime = 0.0;
	double prevTime = 0.0;
	double deltaTime = 0.0;
	double FPS = 0.0;

	std::shared_ptr<Font> boldFont = std::make_shared<Font>("resources/fonts/Montserrat-Bold.ttf", 16);
	std::shared_ptr<Font> regularFont = std::make_shared<Font>("resources/fonts/Montserrat-Regular.ttf", 12);

	double statusUpdateInterval = 0.0;
	std::string statInfo = std::format("FPS {:.3} - {:.4} s", FPS, deltaTime * 1000.0);
	TextParameter textParameter;
	textParameter.lineSpacing = 1.0f;
	textParameter.kerning = 0.0f;

	TextRenderer::Init();

	std::string testText = R"(TOP MOST
Line 1
Line 2
Line 3
Line 4
BOTTOM MOST)";

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
			TextRenderer::Begin(camera.projection * camera.view);

			glm::vec3 color = glm::vec3(1.0f);
			TextRenderer::DrawText(boldFont.get(), testText, 5, WINDOW_HEIGHT - boldFont->GetFontSize(), 1.0f, color, textParameter);

			// const float yOffset = boldFont->GetFontSize();
			// for (int i = 0; i < 2; ++i)
			// {
			// 	float fontSize = (float)regularFont->GetFontSize();
			// 	float yPos = fontSize * i;
			// 	TextRenderer::DrawText(regularFont.get(), std::format("Text {}", i), 5, WINDOW_HEIGHT - yOffset - yPos, 1.0f, color);
			// }

			TextRenderer::End();
		}

		window.SwapBuffers();
	}


	TextRenderer::Shutdown();

	return 0;
}
