#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
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

#include "ImGuiContext.h"
#include "Scene/Model.h"
#include "Renderer/CascadedShadowMap.h"
#include "Camera.h"
#include "Renderer/Material.h"
#include "Renderer/Shader.h"
#include "Renderer/UniformBuffer.h"
#include "Renderer/Font.h"
#include "Renderer/Texture.h"
#include "Renderer/Mesh.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/Bloom.h"
#include "Renderer/SSAO.h"
#include "Renderer/Window.h"
#include "Math/Math.hpp"

#include <glm/gtc/matrix_transform.hpp>

constexpr int RENDER_MODE_COLOR = 0;
constexpr int RENDER_MODE_NORMALS = 1;
constexpr int RENDER_MODE_METALLIC = 2;
constexpr int RENDER_MODE_ROUGHNESS = 3;
constexpr int RENDER_MODE_DEPTH = 4;

using namespace flex;

class Screen
{
public:
    Screen()
    {
        Create();
    }

    void Render(uint32_t texture, uint32_t depthTex, const flex::Camera &camera, const flex::PostProcessing &postProcessing)
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
        shader.SetUniform("u_EnableSSAO", postProcessing.enableSSAO ? 1 : 0);
        shader.SetUniform("u_AOIntensity", postProcessing.aoIntensity);
        shader.SetUniform("u_DebugSSAO", postProcessing.debugSSAO ? 1 : 0);
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

        std::vector<uint32_t> indices = 
        {
            0, 1, 2,
            0, 2, 3,
        };

        vertexArray = std::make_shared<VertexArray>();
        vertexBuffer = std::make_shared<VertexBuffer>(vertices.data(), vertices.size() * sizeof(glm::vec2));
        vertexBuffer->SetAttributes({{VertexAttribType::VECTOR_FLOAT_2}}, sizeof(glm::vec2));
        indexBuffer = std::make_shared<IndexBuffer>(indices.data(), static_cast<uint32_t>(indices.size()));
        
        vertexArray->SetVertexBuffer(vertexBuffer);
        vertexArray->SetIndexBuffer(indexBuffer);

        int error = glGetError();
        assert(error == GL_NO_ERROR);

        shader.AddFromFile("Resources/shaders/screen.vertex.glsl", GL_VERTEX_SHADER);
        shader.AddFromFile("Resources/shaders/screen.frag.glsl", GL_FRAGMENT_SHADER);
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

struct ViewportData
{
    Viewport viewport;
    bool isHovered = false;
};

int main(int argc, char** argv)
{
    std::string modelPath;
    std::string skyboxPath;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-model=") == 0 && i + 1 < argc) {
            modelPath = argv[i + 1];
        }
        if (strcmp(argv[i], "-skybox=") == 0 && i + 1 < argc) {
            skyboxPath = argv[i + 1];
        }
    }

    WindowCreateInfo windowCreateInfo;
    windowCreateInfo.fullscreen = false;
    windowCreateInfo.title = "Flex Engine - OpenGL 4.6 Renderer";
    windowCreateInfo.width = 1280;
    windowCreateInfo.height = 640;
    Window window(windowCreateInfo);

    flex::Renderer::Init();

    flex::Camera camera;
    Screen screen;

    camera.target = glm::vec3(0.0f);
    camera.distance = 5.5f;
    camera.yaw = glm::radians(90.0f);
    camera.pitch = 0.0f;

    // Update initial position and matrices
    const auto initialAspect = static_cast<float>(windowCreateInfo.width) / static_cast<float>(windowCreateInfo.height);
    camera.UpdateSphericalPosition();
    camera.UpdateMatrices(initialAspect);

    // Initialize font and text renderer
    Font font("Resources/fonts/Montserrat-Medium.ttf", 12);
    TextRenderer::Init();

    float FPS = 0.0;
    float statusUpdateInterval = 0.0;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    Shader PBRShader;
    PBRShader.AddFromFile("Resources/shaders/pbr.vertex.glsl", GL_VERTEX_SHADER);
    PBRShader.AddFromFile("Resources/shaders/pbr.frag.glsl", GL_FRAGMENT_SHADER);
    PBRShader.Compile();

    // Shadow depth shader (cascaded)
    Shader shadowDepthShader;
    shadowDepthShader.AddFromFile("Resources/shaders/shadow_depth.vert.glsl", GL_VERTEX_SHADER);
    shadowDepthShader.AddFromFile("Resources/shaders/shadow_depth.frag.glsl", GL_FRAGMENT_SHADER);
    shadowDepthShader.Compile();

    Shader skyboxShader;
    skyboxShader.AddFromFile("Resources/shaders/skybox.vertex.glsl", GL_VERTEX_SHADER);
    skyboxShader.AddFromFile("Resources/shaders/skybox.frag.glsl", GL_FRAGMENT_SHADER);
    skyboxShader.Compile();

    TextureCreateInfo createInfo;
    createInfo.flip = false;
    createInfo.format = Format::RGB32F;
    createInfo.clampMode = WrapMode::REPEAT;
    createInfo.filter = FilterMode::LINEAR;

    bool fileExists = std::filesystem::exists(skyboxPath);
    auto environmentTex = std::make_shared<Texture2D>(createInfo, fileExists ? skyboxPath : "Resources/hdr/rogland_clear_night_4k.hdr");

    // Create skybox mesh
    std::shared_ptr<Mesh> skyboxMesh = MeshLoader::CreateSkyboxCube();

    // Load model from glTF file
    Scene scene;
    fileExists = std::filesystem::exists(modelPath);
    scene.AddModel(fileExists ? modelPath : "Resources/models/damaged_helmet.gltf");
    scene.AddModel("Resources/models/scene.glb");

    flex::CameraBuffer cameraData{};
    SceneData sceneData{};
    CascadedShadowMap csm(CascadedQuality::Medium); // uses binding = 3 for UBO

    std::shared_ptr<UniformBuffer> cameraUbo = UniformBuffer::Create(sizeof(flex::CameraBuffer), UNIFORM_BINDING_LOC_CAMERA);
    std::shared_ptr<UniformBuffer> sceneUbo = UniformBuffer::Create(sizeof(SceneData), UNIFORM_BINDING_LOC_SCENE);

    FramebufferCreateInfo framebufferCreateInfo;
    framebufferCreateInfo.width = window.GetWidth();
    framebufferCreateInfo.height = window.GetHeight();
    framebufferCreateInfo.attachments =
    {
        {Format::RGBA16F, FilterMode::LINEAR, WrapMode::REPEAT }, // Main Color (HDR for bloom)
        {Format::DEPTH24STENCIL8}, // Depth Attachment
    };
    auto framebuffer = Framebuffer::Create(framebufferCreateInfo);

    FramebufferCreateInfo viewportFramebufferCreateInfo;
    viewportFramebufferCreateInfo.width = 256;
    viewportFramebufferCreateInfo.height = 256;
    viewportFramebufferCreateInfo.attachments =
    {
        {Format::RGBA8, FilterMode::LINEAR, WrapMode::REPEAT }, // Main Color
        {Format::DEPTH24STENCIL8}, // Depth Attachment
    };
    auto viewportFramebuffer = Framebuffer::Create(viewportFramebufferCreateInfo);

    Bloom bloom(framebufferCreateInfo.width, framebufferCreateInfo.height);
    SSAO ssao(framebufferCreateInfo.width, framebufferCreateInfo.height);

    window.SetFullscreenCallback([&](int width, int height, bool fullscreen)
        {
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
            // Resize framebuffer
            framebuffer->Resize(width, height);
            bloom.Resize(width, height);
            ssao.Resize(width, height);
            screen.inverseProjection = glm::inverse(camera.projection);
        });

    window.SetDropCallback([&](const std::vector<std::string>& paths)
        {
            for (auto& path : paths)
            {
                scene.AddModel(path);
            }
        });

    // Render Here (main scene)
    ViewportData vpData = {};
    vpData.viewport = { 0, 0, static_cast<uint32_t>(windowCreateInfo.width), static_cast<uint32_t>(windowCreateInfo.height) };
    vpData.isHovered = false;

    flex::ImGuiContext imguiContext(&window);

    // window.Show();

    uint64_t prevCount = SDL_GetPerformanceCounter();
    float freq = static_cast<float>(SDL_GetPerformanceFrequency());

    SDL_Event event;
    while (window.IsLooping())
    {
        while (SDL_PollEvent(&event)) {
            window.PollEvents(&event);
            flex::ImGuiContext::PollEvents(&event);
        }

        const uint64_t currentCount = SDL_GetPerformanceCounter();
        const float deltaTime = static_cast<float>(currentCount - prevCount) / freq;
        prevCount = currentCount;
        FPS = 1.0f / deltaTime;

        statusUpdateInterval -= deltaTime;
        if (statusUpdateInterval <= 0.0)
        {
            auto title = std::format("OpenGL - FPS {:.3f} | {:.3f}", FPS, deltaTime * 1000.0f);
            window.SetWindowTitle(title);
            statusUpdateInterval = 1.0;
        }

        if (!vpData.isHovered)
        {
            camera.mouse.scroll = glm::ivec2(0);
            camera.mouse.position = glm::ivec2(0);
        }

        camera.HandleOrbit(deltaTime);
        camera.HandlePan();
        camera.HandleZoom(deltaTime);
        camera.ApplyInertia(deltaTime);
        camera.UpdateMouseState();
        camera.UpdateCameraPosition();

        screen.inverseProjection = glm::inverse(camera.projection);
        camera.lens.focalDistance = camera.distance;

        // Update camera matrices with the current aspect ratio
        const float aspect = static_cast<float>(vpData.viewport.width) / static_cast<float>(vpData.viewport.height);
        camera.UpdateMatrices(aspect > 0.0f ? aspect : 16.0f / 9.0f);
        cameraData.viewProjection = camera.projection * camera.view;
        cameraData.position = glm::vec4(camera.position, 1.0f);
        cameraData.view = camera.view; // new field used by shadows (also u_View uniform separately)
        cameraUbo->SetData(&cameraData, sizeof(cameraData));

        // Update scene data
        sceneUbo->SetData(&sceneData, sizeof(SceneData));

        // Compute a sun / light direction for shadows (matches shader code)
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

        // Resize framebuffer before rendering
        if (framebuffer->GetWidth() != vpData.viewport.width || framebuffer->GetHeight() != vpData.viewport.height)
        {
            // Only resize if dimensions are valid
            if (vpData.viewport.width > 0 && vpData.viewport.height > 0)
            {
                viewportFramebuffer->Resize(vpData.viewport.width, vpData.viewport.height);
                framebuffer->Resize(vpData.viewport.width, vpData.viewport.height);
                bloom.Resize(static_cast<int>(vpData.viewport.width), static_cast<int>(vpData.viewport.height));
                ssao.Resize(static_cast<int>(vpData.viewport.width), static_cast<int>(vpData.viewport.height));
            }
        }

        // Shadow pass (depth only per cascade)
        glEnable(GL_DEPTH_TEST);
        glCullFace(GL_FRONT); // reduce peter-panning
        for (int ci = 0; ci < CascadedShadowMap::NumCascades; ++ci)
        {
            csm.BeginCascade(ci);
            shadowDepthShader.Use();
            shadowDepthShader.SetUniform("u_CascadeIndex", ci);
            for (const auto& model : scene.models)
                model->RenderDepth(shadowDepthShader);
        }
        csm.EndCascade();
        glCullFace(GL_BACK);

        // FIRST PASS: Render to framebuffer
        framebuffer->Bind(vpData.viewport);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

        // Render models first
        glCullFace(GL_BACK);
        PBRShader.Use();
        // Bind cascaded shadow map (binding = 6 in pbr.frag)
        csm.BindTexture(6);
        PBRShader.SetUniform("u_ShadowMap", 6);

        static int debugShadowMode = 0; // 0 off, 1 cascade index, 2 visibilities
        PBRShader.SetUniform("u_DebugShadows", debugShadowMode);

        for (const auto& model : scene.models)
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
            auto skyboxView = glm::mat4(glm::mat3(camera.view));
            auto skyboxMVP = camera.projection * skyboxView;
            skyboxShader.SetUniform("u_Transform", skyboxMVP);
            environmentTex->Bind(0);
            skyboxShader.SetUniform("u_EnvironmentMap", 0);
            skyboxMesh->vertexArray->Bind();
            flex::Renderer::DrawIndexed(skyboxMesh->vertexArray);

            // Restore state
            glCullFace(GL_BACK);
            glDepthFunc(prevDepthFunc);
            glDepthMask(GL_TRUE);
        }

        // SSAO pass (before screen composite) if enabled
        if (camera.postProcessing.enableSSAO)
        {
            ssao.Generate(framebuffer->GetDepthAttachment(), camera.projection, camera.postProcessing.aoRadius,
                camera.postProcessing.aoBias, camera.postProcessing.aoPower);
        }

        // SECOND PASS: Render framebuffer to default framebuffer (screen)
        // Build bloom chain from HDR color before returning to default framebuffer
        if (camera.postProcessing.enableBloom)
        {
            uint32_t hdrTex = framebuffer->GetColorAttachment(0);
            bloom.Build(hdrTex);
        }

        // Skip viewport rendering if dimensions are invalid
        if (vpData.viewport.width > 0 && vpData.viewport.height > 0)
        {
            viewportFramebuffer->Bind(vpData.viewport);
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
                        glBindTextureUnit(3, 0);
                    }
                }
                // Bind SSAO texture (binding=8 in screen shader)
                if (camera.postProcessing.enableSSAO)
                {
                    uint32_t aoTex = ssao.GetAOTexture();
                    glBindTextureUnit(8, aoTex);
                }
                screen.Render(screenTexture, framebuffer->GetDepthAttachment(), camera, camera.postProcessing);
            }
            // Restore depth testing and culling
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, static_cast<int>(window.GetWidth()), static_cast<int>(window.GetHeight()));
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // Finally render UI/text on top
        auto pr = glm::ortho(0.0f, static_cast<float>(vpData.viewport.width), 0.0f, static_cast<float>(vpData.viewport.height));
        TextRenderer::Begin(pr);
        TextRenderer::DrawString(&font, "ABC",
            glm::translate(glm::mat4(1.0f), { 0.0f, 0.0f, 0.0f }),
            glm::vec3(1.0f), {});
        TextRenderer::End();

        flex::ImGuiContext::NewFrame();

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

        ImGui::Begin("Viewport");
        {
            ImVec2 viewportSize = ImGui::GetContentRegionAvail();
            vpData.viewport.width = viewportSize.x;
            vpData.viewport.height = viewportSize.y;

            // Display framebuffer color attachment as image
            const uint32_t colorTex = viewportFramebuffer->GetColorAttachment(0);
            if (colorTex != 0)
            {
                ImGui::Image(colorTex, viewportSize, ImVec2(0, 1), ImVec2(1, 0));
            }
        }
        vpData.isHovered = ImGui::IsWindowHovered();
        ImGui::End();

        if (ImGui::Begin("Models", nullptr))
        {
            for (size_t i = 0; i < scene.models.size(); ++i)
            {
                ImGui::PushID(i);
                const auto& model = scene.models[i];
                std::stringstream ss;
                ss << "Model " << static_cast<int>(i);
                if (ImGui::CollapsingHeader(ss.str().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bool removing = false;
                    if (ImGui::Button("Remove"))
                    {
                        removing = scene.RemoveModel(static_cast<int>(i));
                    }

                    if (removing)
                    {
                        ImGui::PopID();
                        break;
                    }

                    for (const MeshNode& node : model->GetScene().nodes)
                    {
                        ImGui::PushID(node.name.c_str());
                        for (const std::shared_ptr<Mesh>& mesh : node.meshes)
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
                                const auto& mat = mesh->material;
                                std::stringstream matSS;
                                matSS << "Material - \"" << mat->name << "\"";
                                ImGui::SeparatorText(matSS.str().c_str());

                                glm::vec3 factorVec = mat->params.baseColorFactor;
                                if (ImGui::ColorEdit3("Base Color", &factorVec.x)) mat->params.baseColorFactor = glm::vec4(factorVec, 1.0f);
                                factorVec = mat->params.emissiveFactor;
                                if (ImGui::ColorEdit3("Emissive", &factorVec.x)) mat->params.emissiveFactor = glm::vec4(factorVec, 1.0f);
                                ImGui::SliderFloat("Metallic", &mat->params.metallicFactor, 0.0f, 1.0f);
                                ImGui::SliderFloat("Roughness", &mat->params.roughnessFactor, 0.0f, 1.0f);
                                ImGui::SliderFloat("Occlussion", &mat->params.occlusionStrength, 0.0f, 1.0f);

                                static glm::vec2 imageSize = { 64.0f, 64.0f };
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
                static const std::array<const char*, 2> projLabels = { "Perspective", "Orthographic" };
                int projIndex = camera.projectionType == ProjectionType::Perspective ? 0 : 1;
                if (ImGui::Combo("Projection", &projIndex, projLabels.data(), projLabels.size()))
                {
                    camera.projectionType = projIndex == 0 ? ProjectionType::Perspective : ProjectionType::Orthographic;
                    camera.UpdateMatrices(aspect);
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
                auto& data = csm.GetData();
                bool changed = false;
                changed |= ImGui::SliderFloat("Strength", &data.shadowStrength, 0.0f, 1.0f);
                changed |= ImGui::DragFloat("Min Bias", &data.minBias, 0.00001f, 0.0f, 0.01f, "%.6f");
                changed |= ImGui::DragFloat("Max Bias", &data.maxBias, 0.00001f, 0.0f, 0.01f, "%.6f");
                changed |= ImGui::SliderFloat("PCF Radius", &data.pcfRadius, 0.1f, 4.0f);

                static const std::array<const char*, 3> resolutionLabels = { "Low - 1024px", "Medium - 2048px", "High - 4096px" };
                int cascadeQualityIndex = static_cast<int>(csm.GetQuality());

                if (ImGui::Combo("Resolution", &cascadeQualityIndex, resolutionLabels.data(), resolutionLabels.size()))
                {
                    auto quality = static_cast<CascadedQuality>(cascadeQualityIndex);
                    csm.Resize(quality);
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

            ImGui::SeparatorText("SSAO");
            ImGui::Checkbox("Enable SSAO", &camera.postProcessing.enableSSAO);
            ImGui::Checkbox("Debug SSAO", &camera.postProcessing.debugSSAO);
            ImGui::DragFloat("AO Radius", &camera.postProcessing.aoRadius, 0.01f, 0.05f, 5.0f);
            ImGui::DragFloat("AO Bias", &camera.postProcessing.aoBias, 0.001f, 0.0f, 0.2f, "%.4f");
            ImGui::DragFloat("AO Intensity", &camera.postProcessing.aoIntensity, 0.01f, 0.0f, 4.0f);
            ImGui::DragFloat("AO Power", &camera.postProcessing.aoPower, 0.01f, 0.1f, 4.0f);

            if (ImGui::CollapsingHeader("Render Mode", ImGuiTreeNodeFlags_DefaultOpen))
            {
                int mode = static_cast<int>(sceneData.renderMode);
                if (ImGui::RadioButton("Color", mode == RENDER_MODE_COLOR)) mode = RENDER_MODE_COLOR;
                if (ImGui::RadioButton("Normals", mode == RENDER_MODE_NORMALS)) mode = RENDER_MODE_NORMALS;
                if (ImGui::RadioButton("Metallic", mode == RENDER_MODE_METALLIC)) mode = RENDER_MODE_METALLIC;
                if (ImGui::RadioButton("Roughness", mode == RENDER_MODE_ROUGHNESS)) mode = RENDER_MODE_ROUGHNESS;
                if (ImGui::RadioButton("Depth", mode == RENDER_MODE_DEPTH)) mode = RENDER_MODE_DEPTH;
                sceneData.renderMode = static_cast<float>(mode);
            }
        }
        ImGui::End();

        imguiContext.Render();
        window.SwapBuffers();
    }

    flex::ImGuiContext::Shutdown();

    TextRenderer::Shutdown();
    flex::Renderer::Shutdown();

    return 0;
}
