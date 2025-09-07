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

#include "UIHelper.h"

#include <glad/glad.h>
#include <stb_image_write.h>

#include "Scene/Model.h"
#include "Renderer/CascadedShadowMap.h"
#include "Renderer/Material.h"
#include "Renderer/Window.h"
#include "Renderer/Renderer.h"
#include "Renderer/UniformBuffer.h"
#include "Renderer/Shader.h"
#include "Renderer/Font.h"
#include "Renderer/Texture.h"
#include "Renderer/Mesh.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Bloom.h"
#include "Renderer/Gizmo.h"
#include "Math/Math.hpp"

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
		shader.SetUniform("u_EnableBloom", postProcessing.enableBloom ? 1 : 0);
		shader.SetUniform("u_VignetteRadius", postProcessing.vignetteRadius);
		shader.SetUniform("u_VignetteSoftness", postProcessing.vignetteSoftness);
		shader.SetUniform("u_VignetteIntensity", postProcessing.vignetteIntensity);
		shader.SetUniform("u_VignetteColor", postProcessing.vignetteColor);
		shader.SetUniform("u_ChromaticAberrationAmount", postProcessing.chromAbAmount);
		shader.SetUniform("u_ChromaticAberrationRadial", postProcessing.chromAbRadial);

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

struct SceneData
{
	glm::vec4 lightColor = glm::vec4(1.0f);
	glm::vec2 lightAngle = glm::vec2(0.0f, 0.3f); // azimuth, elevation
	float renderMode = RENDER_MODE_COLOR;
	float fogDensity = 0.01f;
	glm::vec4 fogColor = glm::vec4(0.7f, 0.8f, 0.9f, 1.0f); // light blue fog
	float fogStart = 10.0f;
	float fogEnd = 50.0f;
	float padding[2];
};

struct Scene
{
	void AddModel(const std::string &filename)
	{
		auto &model = models.emplace_back();
		model = Model::Create(filename);
	}

	bool RemoveModel(int index)
	{
		if (index >= models.size())
			return false;
		
		models.erase(models.begin() + index);
		return true;
	}

	std::vector<std::shared_ptr<Model>> models;
};

int main(int argc, char **argv)
{
	std::string modelPath, skyboxPath;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-model=") == 0 && i + 1 < argc) {
			modelPath = argv[i+1];
		}
		if (strcmp(argv[i], "-skybox=") == 0 && i + 1 < argc) {
			skyboxPath = argv[i+1];
		}
	}

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
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

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

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	Shader PBRShader;
	PBRShader.AddFromFile("resources/shaders/pbr.vertex.glsl", GL_VERTEX_SHADER);
	PBRShader.AddFromFile("resources/shaders/pbr.frag.glsl", GL_FRAGMENT_SHADER);
	PBRShader.Compile();

	// Shadow depth shader (cascaded)
	Shader shadowDepthShader;
	shadowDepthShader.AddFromFile("resources/shaders/shadow_depth.vert.glsl", GL_VERTEX_SHADER);
	shadowDepthShader.AddFromFile("resources/shaders/shadow_depth.frag.glsl", GL_FRAGMENT_SHADER);
	shadowDepthShader.Compile();

	Shader skyboxShader;
	skyboxShader.AddFromFile("resources/shaders/skybox.vertex.glsl", GL_VERTEX_SHADER);
	skyboxShader.AddFromFile("resources/shaders/skybox.frag.glsl", GL_FRAGMENT_SHADER);
	skyboxShader.Compile();

	TextureCreateInfo createInfo;
	createInfo.flip = false;
	createInfo.format = Format::RGB32F;
	createInfo.clampMode = WrapMode::REPEAT;
	createInfo.filter = FilterMode::LINEAR;

	bool fileExists = std::filesystem::exists(skyboxPath);
	std::shared_ptr<Texture2D> environmentTex = std::make_shared<Texture2D>(createInfo, fileExists ? skyboxPath : "resources/hdr/rogland_clear_night_4k.hdr");

	// Create skybox mesh
	std::shared_ptr<Mesh> skyboxMesh = MeshLoader::CreateSkyboxCube();

	// Load model from glTF file
	Scene scene;
	fileExists = std::filesystem::exists(modelPath);
	scene.AddModel(fileExists ? modelPath : "resources/models/damaged_helmet.gltf");
	scene.AddModel("resources/models/scene.glb");

	CameraBuffer cameraData{};
	SceneData sceneData{};
	CascadedShadowMap csm; // uses binding = 3 for UBO

	std::shared_ptr<UniformBuffer> cameraUbo = UniformBuffer::Create(sizeof(CameraBuffer), UNIFORM_BINDING_LOC_CAMERA);
	std::shared_ptr<UniformBuffer> sceneUbo = UniformBuffer::Create(sizeof(SceneData), UNIFORM_BINDING_LOC_SCENE);

	FramebufferCreateInfo framebufferCreateInfo;
	framebufferCreateInfo.width = window.GetWidth();
	framebufferCreateInfo.height = window.GetHeight();
	framebufferCreateInfo.attachments = 
	{
		{Format::RGBA16F, FilterMode::LINEAR, WrapMode::REPEAT }, // Main Color (HDR for bloom)
		{Format::DEPTH24STENCIL8}, // Depth Attachment
	};
	std::shared_ptr<Framebuffer> framebuffer = Framebuffer::Create(framebufferCreateInfo);
	Bloom bloom(WINDOW_WIDTH, WINDOW_HEIGHT);

	window.SetFullscreenCallback([&](int width, int height, bool fullscreen)
	{
		WINDOW_WIDTH = width;
		WINDOW_HEIGHT = height;

		screen.inverseProjection = glm::inverse(camera.projection);
		
		// Resize framebuffer
		framebuffer->Resize(width, height);
		bloom.Resize(width, height);
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
		bloom.Resize(width, height);
		screen.inverseProjection = glm::inverse(camera.projection);
	});

	window.SetDropCallback([&](const std::vector<std::string> &paths)
	{
		for (auto &path : paths)
		{
			scene.AddModel(path);
		}
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
					shadowDepthShader.Reload();
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
			camera.UpdateMouseState();
			camera.HandleOrbit(deltaTime);
			camera.HandlePan(deltaTime);
			camera.HandleZoom(deltaTime);
			camera.ApplyInertia(deltaTime);
			camera.UpdateCameraPosition();
		}

		screen.inverseProjection = glm::inverse(camera.projection);
		camera.lens.focalDistance = camera.distance;

		// Update gizmo
		gizmo.Update(camera, window.GetHandle(), deltaTime);
		
		// Update camera matrices with current aspect ratio
		float aspect = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
		camera.UpdateMatrices(aspect > 0.0f ? aspect : 16.0 / 9.0f);
		cameraData.viewProjection = camera.projection * camera.view;
		cameraData.position = glm::vec4(camera.position, 1.0f);
		cameraData.view = camera.view; // new field used by shadows (also u_View uniform separately)
		cameraUbo->SetData(&cameraData, sizeof(cameraData));

		// Update scene data
		sceneData.lightAngle.x = sceneData.lightAngle.x; // azimuth
		sceneData.lightAngle.y = sceneData.lightAngle.y; // elevation
		sceneUbo->SetData(&sceneData, sizeof(SceneData));

		// Compute sun / light direction for shadows (matches shader code)
		float azimuth = sceneData.lightAngle.x;
		float elevation = sceneData.lightAngle.y;
		glm::vec3 sunDirection = {
			cos(elevation) * cos(azimuth),
			sin(elevation),
			cos(elevation) * sin(azimuth)
		};
		glm::vec3 lightDirection = glm::normalize(-sunDirection);

		// Update cascaded shadow map matrices & UBO
		csm.Update(camera, lightDirection);

		// Shadow pass (depth only per cascade)
		glEnable(GL_DEPTH_TEST);
		glCullFace(GL_FRONT); // reduce peter-panning
		for (int ci = 0; ci < CascadedShadowMap::NumCascades; ++ci)
		{
			csm.BeginCascade(ci);
			shadowDepthShader.Use();
			shadowDepthShader.SetUniform("u_CascadeIndex", ci);
			for (auto &model : scene.models)
				model->RenderDepth(shadowDepthShader);
		}
		csm.EndCascade();
		glCullFace(GL_BACK);

		// Render Here (main scene)
		Viewport viewport{0, 0, (uint32_t)WINDOW_WIDTH, (uint32_t)WINDOW_HEIGHT};
		
		// FIRST PASS: Render to framebuffer
		framebuffer->Bind(viewport);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

		// Render models first
		glCullFace(GL_BACK);
		PBRShader.Use();
		// Bind cascaded shadow map (binding = 6 in pbr.frag)
		csm.BindTexture(6);
		PBRShader.SetUniform("u_ShadowMap", 6);
		PBRShader.SetUniform("u_View", camera.view);
		static int debugShadowMode = 0; // 0 off, 1 cascade index, 2 visibility
		PBRShader.SetUniform("u_DebugShadows", debugShadowMode);

		for (auto &model : scene.models)
		{
			model->Render(PBRShader, environmentTex);
		}
		
		// Only render on perspective mode
		if (camera.projectionType == ProjectionType::Perspective)
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
		// Build bloom chain from HDR color before returning to default framebuffer
		if (camera.postProcessing.enableBloom)
		{
			uint32_t hdrTex = framebuffer->GetColorAttachment(0);
			bloom.Build(hdrTex);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		glClearColor(0.0, 0.0, 0.0, 1.0);

		// Disable depth testing and culling for screen quad
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		if (uint32_t screenTexture = framebuffer->GetColorAttachment(0))
		{
			if (camera.postProcessing.enableBloom)
			{
				bloom.BindTextures(); // Bind individual mip levels for compatibility
				// Also bind the final high-quality bloom texture to slot 7
				uint32_t bloomTex = bloom.GetBloomTexture();
				if (bloomTex != 0)
				{
					glBindTextureUnit(3, bloomTex);
				}
				else
				{
					// Fallback: unbind the texture unit if no valid bloom texture
					glBindTextureUnit(3, 0);
				}
			}
			screen.Render(screenTexture, framebuffer->GetDepthAttachment(), camera, camera.postProcessing);
		}
		// Restore depth testing and culling
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);

		// Finally render UI/text on top
		glm::mat4 orthoProjection = glm::ortho(0.0f, (float)WINDOW_WIDTH, 0.0f, (float)WINDOW_HEIGHT);
		TextRenderer::Begin(orthoProjection);
		int startY = WINDOW_HEIGHT - 100;
		TextRenderer::DrawString(&font, "ABC",
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

		if (ImGui::Begin("Models", nullptr))
		{
			for (size_t i = 0; i < scene.models.size(); ++i)
			{
				ImGui::PushID(i);
				bool removing = false;
				std::shared_ptr<Model> &model = scene.models[i];
				if (ImGui::CollapsingHeader(std::format("Model {}", static_cast<int>(i)).c_str(), ImGuiTreeNodeFlags_DefaultOpen))
				{
					if (ImGui::Button("Remove"))
					{
						removing = scene.RemoveModel(static_cast<int>(i));
					}

					if (removing)
					{
						ImGui::PopID();
						break;
					}

					for (MeshNode &node : model->GetScene().nodes)
					{
						ImGui::PushID(node.name.c_str());
						for (std::shared_ptr<Mesh> &mesh : node.meshes)
						{
							if (ImGui::CollapsingHeader(node.name.c_str()))
							{
								// ====== Transforms ======
								glm::vec3 translation, scale, skew;
								glm::vec4 perspective;
								glm::quat orientation;
								glm::decompose(mesh->localTransform, scale, orientation, translation, skew, perspective);
								
								glm::vec3 eulerRotation = glm::eulerAngles(orientation);
								eulerRotation = glm::degrees(eulerRotation);

								bool editing = ImGui::DragFloat3("Position", &translation.x, 0.025f);
								editing |= ImGui::DragFloat3("Rotation", &eulerRotation.x, 0.025f);
								editing |= ImGui::DragFloat3("Scale", &scale.x, 0.025f);

								if (editing)
								{
									orientation = glm::quat(glm::radians(eulerRotation));
									mesh->localTransform = glm::translate(glm::mat4(1.0f), translation) * glm::toMat4(orientation) * glm::scale(glm::mat4(1.0f), scale);
								}

								// ====== Material ======
								std::shared_ptr<Material> &mat = mesh->material;
								ImGui::SeparatorText(std::format("Material - \"{}\"", mat->name).c_str());
								
								glm::vec3 factorVec = mat->params.baseColorFactor;
								if (ImGui::ColorEdit3("Base Color", &factorVec.x)) mat->params.baseColorFactor = glm::vec4(factorVec, 1.0f);
								factorVec = mat->params.emissiveFactor;
								if (ImGui::ColorEdit3("Emissive", &factorVec.x)) mat->params.emissiveFactor = glm::vec4(factorVec, 1.0f);
								ImGui::SliderFloat("Metallic", &mat->params.metallicFactor, 0.0f, 1.0f);
								ImGui::SliderFloat("Roughness", &mat->params.roughnessFactor, 0.0f, 1.0f);
								ImGui::SliderFloat("Occlussion", &mat->params.occlusionStrength, 0.0f, 1.0f);

								static glm::vec2 imageSize = {64.0f, 64.0f};
								UIDrawImage(mat->baseColorTexture, imageSize.x, imageSize.y, "BaseColor");
								UIDrawImage(mat->emissiveTexture, imageSize.x, imageSize.y, "Emissive");
								UIDrawImage(mat->normalTexture, imageSize.x, imageSize.y, "Normal");
								UIDrawImage(mat->metallicRoughnessTexture, imageSize.x, imageSize.y, "MetalRough");
								UIDrawImage(mat->occlusionTexture, imageSize.x, imageSize.y, "Occlusion");
							}
						}
						ImGui::PopID();
					}
				}

				ImGui::PopID();
			}
		}
		ImGui::End();

		// Example stats window
		if (ImGui::Begin("Stats", nullptr))
		{
			ImGui::Text("FPS: %.1f", FPS);
			ImGui::Text("Delta ms: %.3f", deltaTime * 1000.0);
			ImGui::Separator();
			// Projection type selector
			{
				const char* projLabels[] = {"Perspective", "Orthographic"};
				int projIndex = camera.projectionType == ProjectionType::Perspective ? 0 : 1;
				if (ImGui::Combo("Projection", &projIndex, projLabels, IM_ARRAYSIZE(projLabels)))
				{
					camera.projectionType = projIndex == 0 ? ProjectionType::Perspective : ProjectionType::Orthographic;
					camera.UpdateMatrices((float)WINDOW_WIDTH / (float)WINDOW_HEIGHT);
				}
				if (camera.projectionType == ProjectionType::Perspective)
				{
					ImGui::SliderFloat("FOV", &camera.fov, 10.0f, 120.0f);
				}
				else
				{
					ImGui::SliderFloat("Ortho Size", &camera.orthoSize, 1.0f, 200.0f);
				}
			}

			ImGui::SeparatorText("Sun");
			ImGui::ColorEdit3("Light Color", &sceneData.lightColor.x);
			ImGui::SliderFloat("Light Intensity", &sceneData.lightColor.w, 0.0f, 10.0f);
			ImGui::SliderFloat("Sun Azimuth", &sceneData.lightAngle.x, 0.0f, 2.0f * glm::pi<float>());
			ImGui::SliderFloat("Sun Elevation", &sceneData.lightAngle.y, -0.5f, 1.5f);

			ImGui::SeparatorText("Fog");
			ImGui::ColorEdit3("Fog Color", &sceneData.fogColor.x);
			ImGui::SliderFloat("Fog Density", &sceneData.fogDensity, 0.0f, 0.1f, "%.4f");
			ImGui::SliderFloat("Fog Start", &sceneData.fogStart, 0.1f, 100.0f);
			ImGui::SliderFloat("Fog End", &sceneData.fogEnd, 1.0f, 200.0f);
			
			ImGui::SeparatorText("Shadows");
			{
				auto &data = csm.GetData();
				static int shadowResolution = 2048;
				bool changed = false;
				changed |= ImGui::SliderFloat("Strength", (float*)&data.shadowStrength, 0.0f, 1.0f);
				changed |= ImGui::DragFloat("Min Bias", (float*)&data.minBias, 0.00001f, 0.0f, 0.01f, "%.6f");
				changed |= ImGui::DragFloat("Max Bias", (float*)&data.maxBias, 0.00001f, 0.0f, 0.01f, "%.6f");
				changed |= ImGui::SliderFloat("PCF Radius", (float*)&data.pcfRadius, 0.1f, 4.0f);
				if (ImGui::Combo("Resolution", &shadowResolution, "1024\0" "2048\0" "4096\0"))
				{
					int res = shadowResolution == 0 ? 1024 : (shadowResolution == 1 ? 2048 : 4096);
					csm.Resize(res);
				}
				ImGui::Separator();
				ImGui::Text("Shadow Debug");
				ImGui::RadioButton("Off##ShadowDbg", &debugShadowMode, 0); ImGui::SameLine();
				ImGui::RadioButton("Cascades", &debugShadowMode, 1); ImGui::SameLine();
				ImGui::RadioButton("Visibility", &debugShadowMode, 2);
				if (changed) csm.Upload();
			}

			ImGui::SeparatorText("Camera");
			ImGui::SliderFloat("Yaw", &camera.yaw, -glm::pi<float>(), glm::pi<float>());
			ImGui::SliderFloat("Pitch", &camera.pitch, -1.5f, 1.5f);
			ImGui::SliderFloat("Distance", &camera.distance, 0.1f, 50.0f);
			ImGui::SliderFloat("Exposure", &camera.lens.exposure, 0.1f, 5.0f, "%.2f");
			ImGui::SliderFloat("Gamma", &camera.lens.gamma, 0.1f, 5.0f, "%.2f");

			ImGui::SeparatorText("DOF");
			ImGui::Checkbox("Enable DOF", &camera.lens.enableDOF);
			ImGui::SliderFloat("Focal Length", &camera.lens.focalLength, 10.0f, 200.0f);
			ImGui::SliderFloat("FStop", &camera.lens.fStop, 0.7f, 16.0f);
			ImGui::SliderFloat("Focus Range", &camera.lens.focusRange, 0.7f, 16.0f);
			ImGui::SliderFloat("Blur Amount", &camera.lens.blurAmount, 0.5f, 20.0f);
			
			ImGui::SeparatorText("Vignette");
			ImGui::Checkbox("Enable Vignette", &camera.postProcessing.enableVignette);
			ImGui::SliderFloat("Vignette Radius", &camera.postProcessing.vignetteRadius, 0.1f, 1.2f);
			ImGui::SliderFloat("Vignette Softness", &camera.postProcessing.vignetteSoftness, 0.001f, 1.0f);
			ImGui::SliderFloat("Vignette Intensity", &camera.postProcessing.vignetteIntensity, 0.0f, 2.0f);
			ImGui::ColorEdit3("Vignette Color", &camera.postProcessing.vignetteColor.x);
			
			ImGui::SeparatorText("Chromatic Aberation");
			ImGui::Checkbox("Enable Chromatic Aberation", &camera.postProcessing.enableChromAb);
			ImGui::SliderFloat("Amount", &camera.postProcessing.chromAbAmount, 0.0f, 0.03f, "%.4f");
			ImGui::SliderFloat("Radial", &camera.postProcessing.chromAbRadial, 0.1f, 3.0f);

			ImGui::SeparatorText("Bloom");
			ImGui::Checkbox("Enable Bloom", &camera.postProcessing.enableBloom);
			ImGui::DragFloat("Threshold", &bloom.settings.threshold, 0.025, 0.0f, FLT_MAX);
			ImGui::DragFloat("Intensity", &bloom.settings.intensity, 0.025, 0.0f, FLT_MAX);
			ImGui::DragFloat("Knee", &bloom.settings.knee, 0.25, 0.0f, FLT_MAX);
			ImGui::DragFloat("Radius", &bloom.settings.radius, 0.025, 0.0f, 1.0f);
			ImGui::SliderInt("Iterations", &bloom.settings.iterations, 1, 8);

			if (ImGui::CollapsingHeader("Render Mode"))
			{
				int mode = (int)sceneData.renderMode;
				if (ImGui::RadioButton("Color", mode == RENDER_MODE_COLOR)) mode = RENDER_MODE_COLOR;
				if (ImGui::RadioButton("Normals", mode == RENDER_MODE_NORMALS)) mode = RENDER_MODE_NORMALS;
				if (ImGui::RadioButton("Metallic", mode == RENDER_MODE_METALLIC)) mode = RENDER_MODE_METALLIC;
				if (ImGui::RadioButton("Roughness", mode == RENDER_MODE_ROUGHNESS)) mode = RENDER_MODE_ROUGHNESS;
				if (ImGui::RadioButton("Depth", mode == RENDER_MODE_DEPTH)) mode = RENDER_MODE_DEPTH;
				sceneData.renderMode = (float)mode;
			}
		}
		ImGui::End();

		// Render ImGui after swap preparation but before loop ends
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(window.GetHandle());
        }

		window.SwapBuffers();
	}

	// Shutdown ImGui
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	TextRenderer::Shutdown();

	return 0;
}
