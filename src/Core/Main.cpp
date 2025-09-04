#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <array>
#include <cassert>
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
#include "Math/Math.hpp"

#include "Camera.hpp"

#define RENDER_MODE_COLOR 0
#define RENDER_MODE_NORMALS 1
#define RENDER_MODE_METALLIC 2
#define RENDER_MODE_ROUGHNESS 3

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
	if (glm::length(camera.panVelocity) > 0.001f) {
		glm::vec3 right = camera.GetRight();
		glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
		
		float panSpeed = camera.distance;
		glm::vec3 panVector = right * (-camera.panVelocity.x * panSpeed) + 
							 worldUp * (camera.panVelocity.y * panSpeed);
		
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

	Window window("Hello OpenGL", WINDOW_WIDTH, WINDOW_HEIGHT);

	Camera camera;
	// Initialize camera with proper spherical coordinates
	camera.target = glm::vec3(0.0f);
	camera.distance = 5.5f;
	camera.yaw = glm::radians(90.0f);
	camera.pitch = 0.0f;
	
	// Update initial position and matrices
	float initialAspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
	camera.UpdateSphericalPosition();
	camera.UpdateMatrices(initialAspect);

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

		float aspect = (float)width / (float)height;
		// Camera matrices will be updated in the main loop
	});

	double currentTime = 0.0;
	double prevTime = 0.0;
	double deltaTime = 0.0;
	double FPS = 0.0;
	double statusUpdateInterval = 0.0;

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	Shader defaultShader;
	defaultShader.AddFromFile("resources/shaders/default.vertex.glsl", GL_VERTEX_SHADER);
	defaultShader.AddFromFile("resources/shaders/default.frag.glsl", GL_FRAGMENT_SHADER);
	defaultShader.Compile();

	Shader skyboxShader;
	skyboxShader.AddFromFile("resources/shaders/skybox.vertex.glsl", GL_VERTEX_SHADER);
	skyboxShader.AddFromFile("resources/shaders/skybox.frag.glsl", GL_FRAGMENT_SHADER);
	skyboxShader.Compile();

	TextureCreateInfo createInfo;
	createInfo.format = TextureFormat::RGBAF32;
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

		if (glfwGetKey(window.GetHandle(), GLFW_KEY_LEFT_CONTROL) && glfwGetKey(window.GetHandle(), GLFW_KEY_R))
			defaultShader.Reload();

		if (glfwGetKey(window.GetHandle(), GLFW_KEY_1) == GLFW_PRESS)
			debug.renderMode = RENDER_MODE_COLOR;
		else if (glfwGetKey(window.GetHandle(), GLFW_KEY_2) == GLFW_PRESS)
			debug.renderMode = RENDER_MODE_NORMALS;
		else if (glfwGetKey(window.GetHandle(), GLFW_KEY_3) == GLFW_PRESS)
			debug.renderMode = RENDER_MODE_METALLIC;
		else if (glfwGetKey(window.GetHandle(), GLFW_KEY_4) == GLFW_PRESS)
			debug.renderMode = RENDER_MODE_ROUGHNESS;

		UpdateMouseState(camera, window.GetHandle());
		HandleOrbit(camera, deltaTime);
		HandlePan(camera, deltaTime);
		HandleZoom(camera, deltaTime, window.GetHandle());
		ApplyInertia(camera, deltaTime);
		UpdateCameraPosition(camera);
		
		// Update camera matrices with current aspect ratio
		float aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
		camera.UpdateMatrices(aspect);

		// Render Here
		glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
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
		defaultShader.Use();
		
		
		// Render all loaded meshes with their respective textures
		for (const auto& mesh : meshes)
		{
			if (mesh->material)
			{
				mesh->material->occlusionTexture->Bind(4);
				defaultShader.SetUniform("u_OcclusionTexture", 4);

				mesh->material->normalTexture->Bind(3);
				defaultShader.SetUniform("u_NormalTexture", 3);

				mesh->material->metallicRoughnessTexture->Bind(2);
				defaultShader.SetUniform("u_MetallicRoughnessTexture", 2);

				mesh->material->emissiveTexture->Bind(1);
				defaultShader.SetUniform("u_EmissiveTexture", 1);

				mesh->material->baseColorTexture->Bind(0);
				defaultShader.SetUniform("u_BaseColorTexture", 0);
			}

			// Bind environment last to guarantee it stays on unit 5
			environmentTex->Bind(5);
			defaultShader.SetUniform("u_EnvironmentTexture", 5);
			
			defaultShader.SetUniform("u_Transform", rotation);
			mesh->vertexArray->Bind();
			Renderer::DrawIndexed(mesh->vertexArray);
		}
		

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

		window.SwapBuffers();
	}

	return 0;
}
