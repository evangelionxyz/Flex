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

// Dear ImGui
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <glad/glad.h>
#include <stb_image_write.h>

#include "Scene/Model.h"
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
#define RENDER_MODE_DEPTH 4

class Screen
{
public:
	Screen()
	{
		Create();
	}

	void Render(uint32_t texture, uint32_t depthTex, const Camera &camera, const PostProcessing &postProcessing)
	{
		shader.Use();
		glBindTextureUnit(0, texture);
		shader.SetUniform("u_ColorTexture", 0);
		glBindTextureUnit(1, depthTex);
		shader.SetUniform("u_DepthTexture", 1);

		shader.SetUniform("u_FocalLength", camera.lens.focalLength);
		shader.SetUniform("u_FocalDistance", camera.lens.focalDistance);
		shader.SetUniform("u_FStop", camera.lens.fStop);
		shader.SetUniform("u_FocusRange", camera.lens.focusRange);
		shader.SetUniform("u_BlurAmount", camera.lens.blurAmount);
		shader.SetUniform("u_InverseProjection", inverseProjection);
		shader.SetUniform("u_Exposure", camera.lens.exposure);
		shader.SetUniform("u_Gamma", camera.lens.gamma);
		shader.SetUniform("u_EnableDOF", camera.lens.enableDOF ? 1 : 0);
		shader.SetUniform("u_EnableVignette", postProcessing.enableVignette ? 1 : 0);
		shader.SetUniform("u_EnableChromAb", postProcessing.enableChromAb ? 1 : 0);
		shader.SetUniform("u_VignetteRadius", postProcessing.vignetteRadius);
		shader.SetUniform("u_VignetteSoftness", postProcessing.vignetteSoftness);
		shader.SetUniform("u_VignetteIntensity", postProcessing.vignetteIntensity);
		shader.SetUniform("u_VignetteColor", postProcessing.vignetteColor);
		shader.SetUniform("u_ChromaticAbAmount", postProcessing.chromAbAmount);
		shader.SetUniform("u_ChromaticAbRadual", postProcessing.chromAbRadial);

		vertexArray->Bind();

		// Ensure the index buffer is bound for glDrawElements
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->GetHandle());
    	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);
	}

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

	std::shared_ptr<VertexArray> vertexArray;
	std::shared_ptr<VertexBuffer> vertexBuffer;
	std::shared_ptr<IndexBuffer> indexBuffer;

	Shader shader;
	glm::mat4 inverseProjection = glm::mat4(1.0f);
};

struct DebugData
{
	int renderMode = RENDER_MODE_COLOR;
};

struct TimeData
{
	float time = 0.0f;
};

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

	// Initialize ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Docking
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Keyboard controls
	// Style
	ImGui::StyleColorsDark();
	// Backend init
	ImGui_ImplGlfw_InitForOpenGL(window.GetHandle(), true);
	ImGui_ImplOpenGL3_Init("#version 460");

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
	PBRShader.AddFromFile("resources/shaders/pbr.vertex.glsl", GL_VERTEX_SHADER);
	PBRShader.AddFromFile("resources/shaders/pbr.frag.glsl", GL_FRAGMENT_SHADER);
	PBRShader.Compile();

	Shader skyboxShader;
	skyboxShader.AddFromFile("resources/shaders/skybox.vertex.glsl", GL_VERTEX_SHADER);
	skyboxShader.AddFromFile("resources/shaders/skybox.frag.glsl", GL_FRAGMENT_SHADER);
	skyboxShader.Compile();

	TextureCreateInfo createInfo;
	createInfo.flip = false;
	createInfo.format = Format::RGB32F;
	createInfo.clampMode = WrapMode::REPEAT;
	createInfo.filter = FilterMode::LINEAR;
	std::shared_ptr<Texture2D> environmentTex = std::make_shared<Texture2D>(createInfo, "resources/hdr/klippad_sunrise_2_2k.hdr");

	// Create skybox mesh
	std::shared_ptr<Mesh> skyboxMesh = MeshLoader::CreateSkyboxCube();

	// Load model from glTF file
	std::shared_ptr<Model> model = Model::Create("resources/models/damaged_helmet.gltf");
	glm::mat4 modelTr = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f) , glm::vec3(1.0f, 0.0f, 0.0f));
	model->SetTransform(modelTr);

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
		{Format::RGBA8, FilterMode::LINEAR, WrapMode::REPEAT }, // Main Color
		{Format::DEPTH24STENCIL8}, // Depth Attachment
	};
	std::shared_ptr<Framebuffer> framebuffer = Framebuffer::Create(framebufferCreateInfo);

	window.SetFullscreenCallback([&](int width, int height, bool fullscreen)
	{
		WINDOW_WIDTH = width;
		WINDOW_HEIGHT = height;

		screen.inverseProjection = glm::inverse(camera.projection);
		
		// Resize framebuffer
		framebuffer->Resize(width, height);
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
		screen.inverseProjection = glm::inverse(camera.projection);
	});

	window.SetKeyboardCallback([&](int key, int scancode, int action, int mods)
	{
		switch (action)
		{
			case GLFW_PRESS:
			{
				if (key == GLFW_KEY_F11)
				{
					window.ToggleFullScreen();
				}

				if (glfwGetKey(window.GetHandle(), GLFW_KEY_LEFT_CONTROL) && key == GLFW_KEY_R)
				{
					PBRShader.Reload();
					screen.shader.Reload();
				}
				else if (key == GLFW_KEY_ESCAPE)
				{
					glfwSetWindowShouldClose(window.GetHandle(), 1);
				}
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

		if (!ImGui::GetIO().WantCaptureMouse)
		{
			UpdateMouseState(camera, window.GetHandle());
			HandleOrbit(camera, deltaTime);
			HandlePan(camera, deltaTime);
			HandleZoom(camera, deltaTime, window.GetHandle());
			ApplyInertia(camera, deltaTime);
			UpdateCameraPosition(camera);
		}

		// float fstop = 1.4f;
		// float focalLength = 120.0f;
		// screen.SetDOFParameters(focalLength, camera.distance, fstop, invProj);
		screen.inverseProjection = glm::inverse(camera.projection);
		camera.lens.focalDistance = camera.distance;

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
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

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
		for (int z = -2; z < 2; ++z)
		{
			for (int x = -2; x < 2; ++x)
			{
				model->SetTransform(glm::translate(modelTr, {x * 3.0f, z * 3.0f, 0.0}));
				model->Render(PBRShader, environmentTex);
			}
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
			screen.Render(screenTexture, framebuffer->GetDepthAttachment(), camera, camera.postProcessing);
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

		// Start ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// Dockspace window (invisible host)
		{
			ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
			const ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);
			ImGui::SetNextWindowBgAlpha(0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("DockSpaceHost", nullptr, windowFlags);
			ImGui::PopStyleVar(3);
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
			ImGui::End();
		}

		// Example stats window
		if (ImGui::Begin("Stats"))
		{
			ImGui::Text("FPS: %.1f", FPS);
			ImGui::Text("Delta ms: %.3f", deltaTime * 1000.0);
			ImGui::Separator();
			ImGui::Text("Camera");
			ImGui::SliderFloat("Yaw", &camera.yaw, -glm::pi<float>(), glm::pi<float>());
			ImGui::SliderFloat("Pitch", &camera.pitch, -1.5f, 1.5f);
			ImGui::SliderFloat("Distance", &camera.distance, 0.1f, 50.0f);
			ImGui::SliderFloat("Exposure", &camera.lens.exposure, 0.1f, 5.0f, "%.2f");
			ImGui::SliderFloat("Gamma", &camera.lens.gamma, 0.1f, 5.0f, "%.2f");
			
			ImGui::Separator();
			ImGui::Text("Render Mode");
			int mode = debug.renderMode;
			if (ImGui::RadioButton("Color", mode == RENDER_MODE_COLOR)) mode = RENDER_MODE_COLOR;
			if (ImGui::RadioButton("Normals", mode == RENDER_MODE_NORMALS)) mode = RENDER_MODE_NORMALS;
			if (ImGui::RadioButton("Metallic", mode == RENDER_MODE_METALLIC)) mode = RENDER_MODE_METALLIC;
			if (ImGui::RadioButton("Roughness", mode == RENDER_MODE_ROUGHNESS)) mode = RENDER_MODE_ROUGHNESS;
			if (ImGui::RadioButton("Depth", mode == RENDER_MODE_DEPTH)) mode = RENDER_MODE_DEPTH;
			debug.renderMode = mode;

			ImGui::Separator();
			ImGui::Text("DOF");
			ImGui::Checkbox("Enable DOF", &camera.lens.enableDOF);
			ImGui::SliderFloat("Focal Length", &camera.lens.focalLength, 10.0f, 200.0f);
			ImGui::SliderFloat("FStop", &camera.lens.fStop, 0.7f, 16.0f);
			ImGui::SliderFloat("Focus Range", &camera.lens.focusRange, 0.7f, 16.0f);
			ImGui::SliderFloat("Blur Amount", &camera.lens.blurAmount, 0.5f, 20.0f);
			ImGui::Separator();

			ImGui::Text("Vignette");
			ImGui::Checkbox("Enable Vignette", &camera.postProcessing.enableVignette);
			ImGui::SliderFloat("Vignette Radius", &camera.postProcessing.vignetteRadius, 0.1f, 1.2f);
			ImGui::SliderFloat("Vignette Softness", &camera.postProcessing.vignetteSoftness, 0.001f, 1.0f);
			ImGui::SliderFloat("Vignette Intensity", &camera.postProcessing.vignetteIntensity, 0.0f, 2.0f);
			ImGui::ColorEdit3("Vignette Color", &camera.postProcessing.vignetteColor.x);
			ImGui::Separator();

			ImGui::Text("Chromatic Aberration");
			ImGui::Checkbox("Enable Chrom Ab", &camera.postProcessing.enableChromAb);
			ImGui::SliderFloat("Chrom Amount", &camera.postProcessing.chromAbAmount, 0.0f, 0.03f, "%.4f");
			ImGui::SliderFloat("Chrom Radial", &camera.postProcessing.chromAbRadial, 0.1f, 3.0f);
		}
		ImGui::End();

		// Render ImGui after swap preparation but before loop ends
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		window.SwapBuffers();
	}

	// Shutdown ImGui
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	TextRenderer::Shutdown();

	return 0;
}
