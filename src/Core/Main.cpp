#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <array>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <memory>

#include <glad/glad.h>
#include <stb_image_write.h>

#include "Renderer/Window.h"
#include "Renderer/Renderer.h"
#include "Renderer/UniformBuffer.h"
#include "Renderer/Shader.h"
#include "Renderer/Font.h"
#include "Renderer/Texture.h"
#include "Renderer/Mesh.h"
#include "Renderer/Framebuffer.h"
#include "Math/Math.hpp"

#include "Renderer/Gizmo.h"

#include <glm/gtc/matrix_transform.hpp>

#define RENDER_MODE_COLOR 0
#define RENDER_MODE_NORMALS 1
#define RENDER_MODE_METALLIC 2
#define RENDER_MODE_ROUGHNESS 3

class Screen
{
public:
	Screen()
	{
		Create();
	}

	void Render(uint32_t texture)
	{
		shader.Use();
		glBindTextureUnit(0, texture);
		shader.SetUniform("texture0", 0);

		vertexArray->Bind();
		// Ensure the index buffer is bound for glDrawElements
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->GetHandle());
    	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
	}

private:
	std::shared_ptr<VertexArray> vertexArray;
	std::shared_ptr<VertexBuffer> vertexBuffer;
	std::shared_ptr<IndexBuffer> indexBuffer;

	Shader shader;
	void Create()
	{
		std::vector<glm::vec2> vertices =
		{
			{-1.0f,-1.0f },
			{-1.0f, 1.0f },
			{ 1.0f, 1.0f },
			{ 1.0f,-1.0f },
		};

		uint32_t indices[] = 
		{
			0, 1, 2,
			0, 2, 3,
		};

		vertexArray = std::make_shared<VertexArray>();
		vertexBuffer = std::make_shared<VertexBuffer>(vertices.data(), vertices.size() * sizeof(glm::vec2));
		vertexBuffer->SetAttributes({{VertexAttribType::VECTOR_FLOAT_2}}, sizeof(glm::vec2));
		indexBuffer = std::make_shared<IndexBuffer>(indices, (uint32_t)std::size(indices));
		
		vertexArray->SetVertexBuffer(vertexBuffer);
		vertexArray->SetIndexBuffer(indexBuffer);

		int error = glGetError();
		assert(error == GL_NO_ERROR);

		shader.AddFromFile("resources/shaders/screen.vertex.glsl", GL_VERTEX_SHADER);
		shader.AddFromFile("resources/shaders/screen.frag.glsl", GL_FRAGMENT_SHADER);
		shader.Compile();
	}
};

struct DebugData
{
	int renderMode = RENDER_MODE_COLOR;
};

struct TimeData
{
	float time = 0.0f;
};

// Forward declarations for camera functions
void UpdateMouseState(Camera &camera, GLFWwindow *window);
void HandleOrbit(Camera &camera, float deltaTime);
void HandlePan(Camera &camera, float deltaTime);
void HandleZoom(Camera &camera, float deltaTime, GLFWwindow *window);
void ApplyInertia(Camera &camera, float deltaTime);
void UpdateCameraPosition(Camera &camera);

void UpdateCamera(Camera &camera, GLFWwindow *window, float deltaTime, int windowWidth, int windowHeight)
{
	// Update mouse state
	UpdateMouseState(camera, window);
	
	// Handle input
	HandleOrbit(camera, deltaTime);
	HandlePan(camera, deltaTime);
	HandleZoom(camera, deltaTime, window);
	
	// Apply inertia if enabled
	if (camera.controls.enableInertia) {
		ApplyInertia(camera, deltaTime);
	}
	
	// Update camera position and matrices
	UpdateCameraPosition(camera);
	
	float aspect = (float)windowWidth / (float)windowHeight;
	camera.UpdateMatrices(aspect);
}

void UpdateMouseState(Camera &camera, GLFWwindow *window)
{
	// Get current mouse position
	double mouseX, mouseY;
	glfwGetCursorPos(window, &mouseX, &mouseY);
	
	// Store last position before updating
	camera.mouse.lastPosition = camera.mouse.position;
	
	// Update current position
	camera.mouse.position = glm::vec2((float)mouseX, (float)mouseY);
	
	// Update button states
	camera.mouse.leftButton = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
	camera.mouse.middleButton = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
	camera.mouse.rightButton = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
}

void HandleOrbit(Camera &camera, float deltaTime)
{
	if (camera.mouse.leftButton) {
		glm::vec2 delta = camera.mouse.position - camera.mouse.lastPosition;
		
		// Apply mouse movement to angular velocity for inertia
		if (camera.controls.enableInertia) {
			camera.angularVelocity.x += delta.x * camera.controls.mouseSensitivity;
			camera.angularVelocity.y += delta.y * camera.controls.mouseSensitivity;
		}
		
		// Directly update angles for immediate response
		camera.yaw += delta.x * camera.controls.mouseSensitivity;
		camera.pitch += delta.y * camera.controls.mouseSensitivity;
		
		// Clamp pitch to prevent camera flipping
		camera.pitch = glm::clamp(camera.pitch, camera.controls.minPitch, camera.controls.maxPitch);
	}
}

void HandlePan(Camera &camera, float deltaTime)
{
	if (camera.mouse.middleButton) {
		glm::vec2 delta = camera.mouse.position - camera.mouse.lastPosition;
		
		// Calculate pan direction in camera space
		glm::vec3 right = camera.GetRight();
		glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
		
		// Pan in the camera's right and world up directions
		float panSpeed = camera.controls.panSensitivity * camera.distance;
		glm::vec3 panVector = right * (-delta.x * panSpeed) + 
							 worldUp * (delta.y * panSpeed);
		
		// Apply pan to target
		camera.target += panVector;
		
		// Apply to pan velocity for inertia
		if (camera.controls.enableInertia) {
			camera.panVelocity = delta * camera.controls.panSensitivity;
		}
	}
}

void HandleZoom(Camera &camera, float deltaTime, GLFWwindow *window)
{
	// Handle mouse wheel
	float wheelDelta = 0.0f;
	
	// Check for scroll wheel input
	if (camera.mouse.scroll.y != 0) {
		wheelDelta = camera.mouse.scroll.y;
		// Reset scroll after processing
		camera.mouse.scroll.y = 0;
	}
	
	// Handle keyboard zoom controls
	if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS || 
		glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS) {
		wheelDelta -= camera.controls.zoomSensitivity * deltaTime * 10.0f;
	}
	if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS || 
		glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) {
		wheelDelta += camera.controls.zoomSensitivity * deltaTime * 10.0f;
	}
	
	if (wheelDelta != 0.0f) {
		// Apply zoom velocity for smooth zooming
		if (camera.controls.enableInertia) {
			camera.zoomVelocity += wheelDelta * camera.controls.zoomSensitivity;
		} else {
			// Direct zoom for immediate response
			camera.distance -= wheelDelta * camera.controls.zoomSensitivity;
			camera.distance = glm::clamp(camera.distance, camera.controls.minDistance, camera.controls.maxDistance);
		}
	}
	
	// Apply zoom velocity
	if (camera.controls.enableInertia && abs(camera.zoomVelocity) > 0.001f) {
		camera.distance -= camera.zoomVelocity * deltaTime * 10.0f;
		camera.distance = glm::clamp(camera.distance, camera.controls.minDistance, camera.controls.maxDistance);
		
		// Dampen zoom velocity
		camera.zoomVelocity *= camera.controls.zoomDamping;
		
		// Stop very small velocities
		if (abs(camera.zoomVelocity) < 0.001f) {
			camera.zoomVelocity = 0.0f;
		}
	}
}

void ApplyInertia(Camera &camera, float deltaTime)
{
	// Apply angular inertia
	if (glm::length(camera.angularVelocity) > 0.001f) {
		camera.yaw += camera.angularVelocity.x * deltaTime;
		camera.pitch += camera.angularVelocity.y * deltaTime;
		camera.pitch = glm::clamp(camera.pitch, camera.controls.minPitch, camera.controls.maxPitch);
		
		// Dampen angular velocity
		camera.angularVelocity *= camera.controls.inertiaDamping;
		
		// Stop very small velocities
		if (glm::length(camera.angularVelocity) < 0.001f) {
			camera.angularVelocity = glm::vec2(0.0f);
		}
	}
	
	// Apply pan inertia
	if (glm::length(camera.panVelocity) > 0.001f)
	{
		glm::vec3 right = camera.GetRight();
		glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
		
		float panSpeed = camera.distance;
		glm::vec3 panVector = right * (-camera.panVelocity.x * panSpeed) + worldUp * (camera.panVelocity.y * panSpeed);
		
		camera.target += panVector * deltaTime;
		
		// Dampen pan velocity
		camera.panVelocity *= camera.controls.inertiaDamping;
		
		// Stop very small velocities
		if (glm::length(camera.panVelocity) < 0.001f) {
			camera.panVelocity = glm::vec2(0.0f);
		}
	}
}

void UpdateCameraPosition(Camera &camera)
{
	// Update camera position based on spherical coordinates
	camera.UpdateSphericalPosition();
	
	// Update view matrix
	camera.view = glm::lookAt(camera.position, camera.target, camera.up);
}

int main()
{
	int WINDOW_WIDTH = 800;
	int WINDOW_HEIGHT = 650;

	WindowCreateInfo windowCreateInfo;
	windowCreateInfo.fullscreen = true;
	windowCreateInfo.title = "PBR";
	windowCreateInfo.width = WINDOW_WIDTH;
	windowCreateInfo.height = WINDOW_HEIGHT;
	Window window(windowCreateInfo);

	Camera camera;
	Gizmo gizmo;
	Screen screen;

	// Initialize gizmo at origin
	gizmo.SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
	gizmo.SetMode(GizmoMode::TRANSLATE);
	// Initialize camera with proper spherical coordinates
	camera.target = glm::vec3(0.0f);
	camera.distance = 5.5f;
	camera.yaw = glm::radians(90.0f);
	camera.pitch = 0.0f;
	
	// Update initial position and matrices
	float initialAspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
	camera.UpdateSphericalPosition();
	camera.UpdateMatrices(initialAspect);

	// Initialize font and text renderer
	Font font("resources/fonts/Montserrat-Medium.ttf", 12);
	TextRenderer::Init();

	double currentTime = 0.0;
	double prevTime = 0.0;
	double deltaTime = 0.0;
	double FPS = 0.0;
	double statusUpdateInterval = 0.0;
	double debugTextUpdateInterval = 0.0;
	int frameCount = 0;
	std::string debugText[10];
	
	// Initialize debug text
	for (int i = 0; i < 10; ++i)
	{
		debugText[i] = std::format("Initializing... {}", i);
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	Shader PBRShader;
	PBRShader.AddFromFile("resources/shaders/default.vertex.glsl", GL_VERTEX_SHADER);
	PBRShader.AddFromFile("resources/shaders/default.frag.glsl", GL_FRAGMENT_SHADER);
	PBRShader.Compile();

	Shader skyboxShader;
	skyboxShader.AddFromFile("resources/shaders/skybox.vertex.glsl", GL_VERTEX_SHADER);
	skyboxShader.AddFromFile("resources/shaders/skybox.frag.glsl", GL_FRAGMENT_SHADER);
	skyboxShader.Compile();

	TextureCreateInfo createInfo;
	createInfo.flip = false;
	createInfo.format = Format::RGB32F;
	createInfo.clampMode = ClampMode::REPEAT;
	createInfo.filter = FilterMode::LINEAR;
	std::shared_ptr<Texture2D> environmentTex = std::make_shared<Texture2D>(createInfo, "resources/hdr/klippad_sunrise_2_2k.hdr");

	// Create skybox mesh
	std::shared_ptr<Mesh> skyboxMesh = MeshLoader::CreateSkyboxCube();

	// Load meshes from glTF file
	std::vector<std::shared_ptr<Mesh>> meshes = MeshLoader::LoadFromGLTF("resources/models/helmet.glb");
	
	// Fallback if no meshes were loaded
	if (meshes.empty())
	{
		std::cout << "No meshes loaded from glTF, using fallback quad\n";
		meshes.push_back(MeshLoader::CreateFallbackQuad());
	}

	glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f) , glm::vec3(1.0f, 0.0f, 0.0f));

	CameraBuffer cameraData;
	DebugData debug;
	TimeData timeData;

	std::shared_ptr<UniformBuffer> cameraUbo = UniformBuffer::Create(sizeof(CameraBuffer), 0);
	std::shared_ptr<UniformBuffer> debugUbo = UniformBuffer::Create(sizeof(DebugData), 1);
	std::shared_ptr<UniformBuffer> timeUbo = UniformBuffer::Create(sizeof(TimeData), 2);

	FramebufferCreateInfo framebufferCreateInfo;
	framebufferCreateInfo.width = window.GetWidth();
	framebufferCreateInfo.height = window.GetHeight();
	framebufferCreateInfo.attachments = 
	{
		{Format::RGBA8, FilterMode::LINEAR, ClampMode::REPEAT }, // Main Color
		{Format::DEPTH24STENCIL8}, // Depth Attachment
	};
	std::shared_ptr<Framebuffer> framebuffer = Framebuffer::Create(framebufferCreateInfo);

	window.SetFullscreenCallback([&](int width, int height, bool fullscreen)
	{
		WINDOW_WIDTH = width;
		WINDOW_HEIGHT = height;
		
		// Resize framebuffer
		// framebuffer->Resize(width, height);
	});

	window.SetScrollCallback([&](int xOffset, int yOffset)
	{
		// Store scroll delta for processing in the update loop
		camera.mouse.scroll.x = xOffset;
		camera.mouse.scroll.y = yOffset;
	});

	window.SetResizeCallback([&](int width, int height)
	{
		WINDOW_WIDTH = width;
		WINDOW_HEIGHT = height;
		
		// Resize framebuffer
		framebuffer->Resize(width, height);
	});

	window.SetKeyboardCallback([&](int key, int scancode, int action, int mods)
	{
		switch (action)
		{
			case GLFW_PRESS:
			{
				if (key == GLFW_KEY_F11)
					window.ToggleFullScreen();

				if (glfwGetKey(window.GetHandle(), GLFW_KEY_LEFT_CONTROL) && key == GLFW_KEY_R)
					PBRShader.Reload();

				if (key == GLFW_KEY_1 )
					debug.renderMode = RENDER_MODE_COLOR;
				else if (key == GLFW_KEY_2 )
					debug.renderMode = RENDER_MODE_NORMALS;
				else if (key == GLFW_KEY_3 )
					debug.renderMode = RENDER_MODE_METALLIC;
				else if (key == GLFW_KEY_4 )
					debug.renderMode = RENDER_MODE_ROUGHNESS;
				break;
			}
			case GLFW_REPEAT:
			{
				break;
			}
		}
	});

	window.Show();

	while (window.IsLooping())
	{
		currentTime = glfwGetTime();
		deltaTime = currentTime - prevTime;
		prevTime = currentTime;
		FPS = 1.0 / deltaTime;

		statusUpdateInterval -= deltaTime;
		if (statusUpdateInterval <= 0.0)
		{
			std::string statInfo = std::format("OpenGL - FPS {:.3} | {:.3} ms Camera Y: {:.3} P: {:.3}", FPS, deltaTime * 1000.0, camera.yaw, camera.pitch);
			window.SetWindowTitle(statInfo);
			statusUpdateInterval = 1.0;
		}

		// Gizmo mode switching
		if (glfwGetKey(window.GetHandle(), GLFW_KEY_Q) == GLFW_PRESS)
			gizmo.SetMode(GizmoMode::TRANSLATE);
		else if (glfwGetKey(window.GetHandle(), GLFW_KEY_W) == GLFW_PRESS)
			gizmo.SetMode(GizmoMode::ROTATE);
		else if (glfwGetKey(window.GetHandle(), GLFW_KEY_E) == GLFW_PRESS)
			gizmo.SetMode(GizmoMode::SCALE);

		UpdateMouseState(camera, window.GetHandle());
		HandleOrbit(camera, deltaTime);
		HandlePan(camera, deltaTime);
		HandleZoom(camera, deltaTime, window.GetHandle());
		ApplyInertia(camera, deltaTime);
		UpdateCameraPosition(camera);
		
		// Update gizmo
		gizmo.Update(camera, window.GetHandle(), deltaTime);
		
		// Update camera matrices with current aspect ratio
		float aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
		camera.UpdateMatrices(aspect > 0.0f ? aspect : 16.0 / 9.0f);

		// Render Here
		Viewport viewport{0, 0, (uint32_t)WINDOW_WIDTH, (uint32_t)WINDOW_HEIGHT};
		
		// FIRST PASS: Render to framebuffer
		// framebuffer->CheckSize((uint32_t)WINDOW_WIDTH, (uint32_t)WINDOW_HEIGHT);
		framebuffer->Bind(viewport);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		glClearColor(0.1f, 0.1f, 0.15f, 1.0f);

		// Update camera data
		cameraData.viewProjection = camera.projection * camera.view;
		cameraData.position = glm::vec4(camera.position, 1.0f);
		cameraUbo->SetData(&cameraData, sizeof(cameraData), 0);
		
		// Update time data
		timeData.time = (float)currentTime;
		timeUbo->SetData(&timeData, sizeof(timeData));
		
		debugUbo->SetData(&debug, sizeof(debug));

		// Render models first
		glCullFace(GL_BACK);
		PBRShader.Use();
		
		// Render all loaded meshes with their respective textures
		for (const auto& mesh : meshes)
		{
			if (mesh->material)
			{
				mesh->material->occlusionTexture->Bind(4);
				PBRShader.SetUniform("u_OcclusionTexture", 4);

				mesh->material->normalTexture->Bind(3);
				PBRShader.SetUniform("u_NormalTexture", 3);

				mesh->material->metallicRoughnessTexture->Bind(2);
				PBRShader.SetUniform("u_MetallicRoughnessTexture", 2);

				mesh->material->emissiveTexture->Bind(1);
				PBRShader.SetUniform("u_EmissiveTexture", 1);

				mesh->material->baseColorTexture->Bind(0);
				PBRShader.SetUniform("u_BaseColorTexture", 0);
			}

			// Bind environment last to guarantee it stays on unit 5
			environmentTex->Bind(5);
			PBRShader.SetUniform("u_EnvironmentTexture", 5);
			
			PBRShader.SetUniform("u_Transform", rotation);
			mesh->vertexArray->Bind();
			Renderer::DrawIndexed(mesh->vertexArray);
		}

		{
			// Render skybox last (no depth writes, pass when depth equals far plane)
			glDepthMask(GL_FALSE);
			GLint prevDepthFunc; glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);
			glDepthFunc(GL_LEQUAL);

			glCullFace(GL_FRONT);
			skyboxShader.Use();
			// Create skybox transformation (remove translation from view)
			glm::mat4 skyboxView = glm::mat4(glm::mat3(camera.view));
			glm::mat4 skyboxMVP = camera.projection * skyboxView;
			skyboxShader.SetUniform("u_Transform", skyboxMVP);
			environmentTex->Bind(0);
			skyboxShader.SetUniform("u_EnvironmentMap", 0);
			skyboxMesh->vertexArray->Bind();
			Renderer::DrawIndexed(skyboxMesh->vertexArray);

			// Restore state
			glCullFace(GL_BACK);
			glDepthFunc(prevDepthFunc);
			glDepthMask(GL_TRUE);
		}

		// SECOND PASS: Render framebuffer to default framebuffer (screen)
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		glClearColor(0.0, 0.0, 0.0, 1.0);

		// Disable depth testing and culling for screen quad
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		if (uint32_t screenTexture = framebuffer->GetColorAttachment(0))
		{
			screen.Render(screenTexture);
		}
		// Restore depth testing and culling
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		// Finally render UI/text on top
		glm::mat4 orthoProjection = glm::ortho(0.0f, (float)WINDOW_WIDTH, 0.0f, (float)WINDOW_HEIGHT);
		TextRenderer::Begin(orthoProjection);
		int startY = WINDOW_HEIGHT - 100;
		TextRenderer::DrawString(&font, "Evangelion Manuhutu",
			glm::translate(glm::mat4(1.0f), {0.0f, 0.0f, 0.0f}),
			glm::vec3(1.0f), {});
		TextRenderer::End();

		window.SwapBuffers();
	}

	TextRenderer::Shutdown();

	return 0;
}
