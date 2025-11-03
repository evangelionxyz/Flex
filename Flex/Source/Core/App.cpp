// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "App.h"

namespace flex
{
    App::App(int argc, char** argv)
    {
        WindowCreateInfo windowCI;
        windowCI.fullscreen = false;
        windowCI.title = "Flex Engine - OpenGL 4.6 Renderer";
        windowCI.width = 1280;
        windowCI.height = 720;
        m_Window = CreateRef<Window>(windowCI);

        // Initialize Window Callbacks
        m_Window->SetMouseMotionCallback([this](const glm::vec2 &position, const glm::vec2 &delta) { App::OnMouseMotion(position, delta); });
        m_Window->SetScrollCallback([this](float x, float y) { App::OnMouseScroll(x, y); });

        // Initialize Renderer
        Renderer::Init();

        m_Camera.target = glm::vec3(0.0f);
        m_Camera.distance = 5.5f;
        m_Camera.yaw = glm::radians(90.0f);
        m_Camera.pitch = 0.0f;

        // Update initial position and matrices
        const auto initialAspect = static_cast<float>(m_Window->GetWidth()) / static_cast<float>(m_Window->GetHeight());
        m_Camera.UpdateMatrices(initialAspect);

        // Initialize font and text renderer
        Font font("Resources/fonts/Montserrat-Medium.ttf", 12);
        TextRenderer::Init();

        m_Screen = CreateRef<Screen>();
    }

    App::~App()
    {
        ImGuiContext::Shutdown();
        TextRenderer::Shutdown();
        Renderer::Shutdown();
    }

    void App::Run()
    {
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

        m_EnvMap = CreateRef<Texture2D>(createInfo, "Resources/hdr/rogland_clear_night_4k.hdr");

        // Create skybox mesh
        auto skyboxMesh = MeshLoader::CreateSkyboxCube();

        // Load model from glTF file
        m_Scene.AddModel("Resources/models/damaged_helmet.gltf");
        m_Scene.AddModel("Resources/models/scene.glb");

        CameraBuffer cameraData{};
        m_CSM = CreateRef<CascadedShadowMap>(CascadedQuality::Medium); // uses binding = 3 for UBO

        std::shared_ptr<UniformBuffer> cameraUbo = UniformBuffer::Create(sizeof(CameraBuffer), UNIFORM_BINDING_LOC_CAMERA);
        std::shared_ptr<UniformBuffer> sceneUbo = UniformBuffer::Create(sizeof(SceneData), UNIFORM_BINDING_LOC_SCENE);

        FramebufferCreateInfo sceneFBCreateInfo;
        sceneFBCreateInfo.width = m_Window->GetWidth();
        sceneFBCreateInfo.height = m_Window->GetHeight();
        sceneFBCreateInfo.attachments =
        {
            {Format::RGBA16F, FilterMode::LINEAR, WrapMode::REPEAT }, // Main Color (HDR for bloom)
            {Format::DEPTH24STENCIL8}, // Depth Attachment
        };
        m_SceneFB = Framebuffer::Create(sceneFBCreateInfo);

        FramebufferCreateInfo viewportFBCreateInfo;
        viewportFBCreateInfo.width = m_Window->GetWidth();
        viewportFBCreateInfo.height = m_Window->GetHeight();
        viewportFBCreateInfo.attachments =
        {
            {Format::RGBA8, FilterMode::LINEAR, WrapMode::REPEAT }, // Main Color
            {Format::DEPTH24STENCIL8}, // Depth Attachment
        };
        m_ViewportFB = Framebuffer::Create(viewportFBCreateInfo);

        m_Bloom = CreateRef<Bloom>(viewportFBCreateInfo.width, viewportFBCreateInfo.height);
        m_SSAO = CreateRef<SSAO>(viewportFBCreateInfo.width, viewportFBCreateInfo.height);

        // Render Here (main scene)
        m_Vp.viewport = { 0, 0, static_cast<uint32_t>(viewportFBCreateInfo.width), static_cast<uint32_t>(viewportFBCreateInfo.height) };
        m_Vp.isHovered = false;

        ImGuiContext imguiContext(m_Window.get());

        uint64_t prevCount = SDL_GetPerformanceCounter();
        float freq = static_cast<float>(SDL_GetPerformanceFrequency());
        float statusUpdateInterval = 0.0;

        SDL_Event event;
        while (m_Window->IsLooping())
        {
            while (SDL_PollEvent(&event))
            {
                m_Window->PollEvents(&event);
                ImGuiContext::PollEvents(&event);
            }

            const uint64_t currentCount = SDL_GetPerformanceCounter();
            m_FrameData.deltaTime = static_cast<float>(currentCount - prevCount) / freq;
            prevCount = currentCount;
            m_FrameData.fps = 1.0f / m_FrameData.deltaTime;

            statusUpdateInterval -= m_FrameData.deltaTime;
            if (statusUpdateInterval <= 0.0)
            {
                auto title = std::format("Flex Engine - OpenGL 4.6 Renderer FPS {:.3f} | {:.3f}", m_FrameData.fps, m_FrameData.deltaTime * 1000.0f);
                m_Window->SetWindowTitle(title);
                statusUpdateInterval = 1.0;
            }

            m_Camera.OnUpdate(m_FrameData.deltaTime);
            
            m_Screen->inverseProjection = glm::inverse(m_Camera.projection);
            m_Camera.lens.focalDistance = m_Camera.distance;

            // Update camera matrices with the current aspect ratio
            const float aspect = static_cast<float>(m_Vp.viewport.width) / static_cast<float>(m_Vp.viewport.height);
            m_Camera.UpdateMatrices(aspect > 0.0f ? aspect : 16.0f / 9.0f);
            cameraData.viewProjection = m_Camera.projection * m_Camera.view;
            cameraData.position = glm::vec4(m_Camera.position, 1.0f);
            cameraData.view = m_Camera.view; // new field used by shadows (also u_View uniform separately)
            cameraUbo->SetData(&cameraData, sizeof(cameraData));

            // Update scene data
            sceneUbo->SetData(&m_SceneData, sizeof(SceneData));

            // Compute a sun / light direction for shadows (matches shader code)
            glm::vec3 sunDirection = {
                cos(m_SceneData.lightAngle.y)* cos(m_SceneData.lightAngle.x),
                sin(m_SceneData.lightAngle.y),
                cos(m_SceneData.lightAngle.y)* sin(m_SceneData.lightAngle.x)
            };
            glm::vec3 lightDirection = glm::normalize(-sunDirection);

            // Update cascaded shadow map matrices & UBO
            m_CSM->Update(m_Camera, lightDirection);

            // Resize framebuffer before rendering
            if (m_SceneFB->GetWidth() != m_Vp.viewport.width || m_SceneFB->GetHeight() != m_Vp.viewport.height)
            {
                // Only resize if dimensions are valid
                if (m_Vp.viewport.width > 0 && m_Vp.viewport.height > 0)
                {
                    m_ViewportFB->Resize(m_Vp.viewport.width, m_Vp.viewport.height);
                    m_SceneFB->Resize(m_Vp.viewport.width, m_Vp.viewport.height);
                    m_Bloom->Resize(static_cast<int>(m_Vp.viewport.width), static_cast<int>(m_Vp.viewport.height));
                    m_SSAO->Resize(static_cast<int>(m_Vp.viewport.width), static_cast<int>(m_Vp.viewport.height));
                }
            }

            // Shadow pass (depth only per cascade)
            glEnable(GL_DEPTH_TEST);
            glCullFace(GL_FRONT); // reduce peter-panning
            for (int ci = 0; ci < CascadedShadowMap::NumCascades; ++ci)
            {
                m_CSM->BeginCascade(ci);
                shadowDepthShader.Use();
                shadowDepthShader.SetUniform("u_CascadeIndex", ci);
                for (const auto& model : m_Scene.models)
                {
                    model->RenderDepth(shadowDepthShader);
                }
            }
            m_CSM->EndCascade();
            glCullFace(GL_BACK);

            // FIRST PASS: Render to framebuffer
            m_SceneFB->Bind(m_Vp.viewport);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

            // Render models first
            glCullFace(GL_BACK);
            PBRShader.Use();
            // Bind cascaded shadow map (binding = 6 in pbr.frag)
            m_CSM->BindTexture(6);
            PBRShader.SetUniform("u_ShadowMap", 6);
            PBRShader.SetUniform("u_DebugShadows", m_Camera.controls.debugShadowMode);

            for (const auto& model : m_Scene.models)
            {
                model->Render(PBRShader, m_EnvMap);
            }

            // Only render on perspective mode
            if (m_Camera.projectionType == ProjectionType::Perspective)
            {
                // Render skybox last (no depth writes, pass when depth equals far plane)
                glDepthMask(GL_FALSE);
                GLint prevDepthFunc; glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);
                glDepthFunc(GL_LEQUAL);

                glCullFace(GL_FRONT);
                skyboxShader.Use();
                // Create skybox transformation (remove translation from view)
                auto skyboxView = glm::mat4(glm::mat3(m_Camera.view));
                auto skyboxMVP = m_Camera.projection * skyboxView;
                skyboxShader.SetUniform("u_Transform", skyboxMVP);
                m_EnvMap->Bind(0);
                skyboxShader.SetUniform("u_EnvironmentMap", 0);
                skyboxMesh->vertexArray->Bind();
                Renderer::DrawIndexed(skyboxMesh->vertexArray);

                // Restore state
                glCullFace(GL_BACK);
                glDepthFunc(prevDepthFunc);
                glDepthMask(GL_TRUE);
            }

            // SSAO pass (before screen composite) if enabled
            if (m_Camera.postProcessing.enableSSAO)
            {
                m_SSAO->Generate(m_SceneFB->GetDepthAttachment(), m_Camera.projection,
                    m_Camera.postProcessing.aoRadius, m_Camera.postProcessing.aoBias,
                    m_Camera.postProcessing.aoPower);
            }

            // SECOND PASS: Render framebuffer to default framebuffer (screen)
            // Build bloom chain from HDR color before returning to default framebuffer
            if (m_Camera.postProcessing.enableBloom)
            {
                uint32_t hdrTex = m_SceneFB->GetColorAttachment(0);
                m_Bloom->Build(hdrTex);
            }

            // Skip viewport rendering if dimensions are invalid
            if (m_Vp.viewport.width > 0 && m_Vp.viewport.height > 0)
            {
                m_ViewportFB->Bind(m_Vp.viewport);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
                glClearColor(0.0, 0.0, 0.0, 1.0);

                // Disable depth testing and culling for screen quad
                glDisable(GL_DEPTH_TEST);
                glDisable(GL_CULL_FACE);
                if (uint32_t screenTexture = m_SceneFB->GetColorAttachment(0))
                {
                    if (m_Camera.postProcessing.enableBloom)
                    {
                        // Bind individual mip levels for compatibility
                        m_Bloom->BindTextures(); 

                        // Also bind the final high-quality bloom texture to slot 7
                        uint32_t bloomTex = m_Bloom->GetBloomTexture();
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
                    if (m_Camera.postProcessing.enableSSAO)
                    {
                        uint32_t aoTex = m_SSAO->GetAOTexture();
                        glBindTextureUnit(8, aoTex);
                    }
                    m_Screen->Render(screenTexture, m_SceneFB->GetDepthAttachment(), m_Camera, m_Camera.postProcessing);
                }

                // Restore depth testing and culling
                glEnable(GL_DEPTH_TEST);
                glEnable(GL_CULL_FACE);
            }

            // =========================================
            // ======== Render Main Framebuffer ========

            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, static_cast<int>(m_Window->GetWidth()), static_cast<int>(m_Window->GetHeight()));
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

            ImGuiContext::NewFrame();
            {
                // Dockspace window (invisible host)
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
            OnImGuiRender();
            ImGuiContext::Render();

            m_Window->SwapBuffers();
        }
    }

    void App::OnImGuiRender()
    {
        UIViewport();
        UISettings();
        UISceneHierarchy();
        UISceneProperties();
    }

    void App::UIViewport()
    {
        ImGui::Begin("Viewport");
        {
            ImVec2 viewportSize = ImGui::GetContentRegionAvail();
            m_Vp.viewport.width = viewportSize.x;
            m_Vp.viewport.height = viewportSize.y;

            // Display framebuffer color attachment as image
            const uint32_t colorTex = m_ViewportFB->GetColorAttachment(0);
            if (colorTex != 0)
            {
                ImGui::Image(colorTex, viewportSize, ImVec2(0, 1), ImVec2(1, 0));
            }
        }
        m_Vp.isHovered = ImGui::IsWindowHovered();
        ImGui::End();
    }

    void App::UISettings()
    {
        // ============ Scene Settings ============
        if (ImGui::Begin("Settings", nullptr))
        {
            constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

            ImGui::Text("FPS: %.1f", m_FrameData.fps);
            ImGui::Text("Delta ms: %.3f", m_FrameData.deltaTime * 1000.0);

            // ============ Camera Settings ============
            if (ImGui::TreeNodeEx("Camera Settings", treeFlags))
            {
                // Projection type selector
                static const std::array<const char*, 2> projLabels = { "Perspective", "Orthographic" };
                int projIndex = m_Camera.projectionType == ProjectionType::Perspective ? 0 : 1;
                if (ImGui::Combo("Projection", &projIndex, projLabels.data(), projLabels.size()))
                {
                    m_Camera.projectionType = projIndex == 0 ? ProjectionType::Perspective : ProjectionType::Orthographic;

                    const float aspect = static_cast<float>(m_Vp.viewport.width) / static_cast<float>(m_Vp.viewport.height);
                    m_Camera.UpdateMatrices(aspect);
                }
                if (m_Camera.projectionType == ProjectionType::Perspective)
                {
                    ImGui::SliderFloat("FOV", &m_Camera.fov, 10.0f, 120.0f);
                }
                else
                {
                    ImGui::SliderFloat("Ortho Size", &m_Camera.orthoSize, 1.0f, 200.0f);
                }

                ImGui::SeparatorText("Camera");
                ImGui::SliderFloat("Yaw", &m_Camera.yaw, -glm::pi<float>(), glm::pi<float>());
                ImGui::SliderFloat("Pitch", &m_Camera.pitch, -1.5f, 1.5f);
                ImGui::SliderFloat("Distance", &m_Camera.distance, 0.1f, 50.0f);
                ImGui::SliderFloat("Exposure", &m_Camera.lens.exposure, 0.1f, 5.0f, "%.2f");
                ImGui::SliderFloat("Gamma", &m_Camera.lens.gamma, 0.1f, 5.0f, "%.2f");

                ImGui::TreePop();
            }

            // ============ Environment Settings ============
            if (ImGui::TreeNodeEx("Environment", treeFlags))
            {
                // ============ Sun Settings ============
                ImGui::SeparatorText("Sun");
                ImGui::ColorEdit3("Light Color", &m_SceneData.lightColor.x);
                ImGui::SliderFloat("Light Intensity", &m_SceneData.lightColor.w, 0.0f, 10.0f);
                ImGui::SliderFloat("Sun Azimuth", &m_SceneData.lightAngle.x, 0.0f, 2.0f * glm::pi<float>());
                ImGui::SliderFloat("Sun Elevation", &m_SceneData.lightAngle.y, -0.5f, 1.5f);

                // ============ Fog Settings ============
                ImGui::SeparatorText("Fog");
                ImGui::ColorEdit3("Fog Color", &m_SceneData.fogColor.x);
                ImGui::SliderFloat("Fog Density", &m_SceneData.fogDensity, 0.0f, 0.1f, "%.4f");
                ImGui::SliderFloat("Fog Start", &m_SceneData.fogStart, 0.1f, 100.0f);
                ImGui::SliderFloat("Fog End", &m_SceneData.fogEnd, 1.0f, 200.0f);

                // ============ Shadows Settings ============
                ImGui::SeparatorText("Shadows");
                {
                    auto& data = m_CSM->GetData();
                    bool changed = false;
                    changed |= ImGui::SliderFloat("Strength", &data.shadowStrength, 0.0f, 1.0f);
                    changed |= ImGui::DragFloat("Min Bias", &data.minBias, 0.00001f, 0.0f, 0.01f, "%.6f");
                    changed |= ImGui::DragFloat("Max Bias", &data.maxBias, 0.00001f, 0.0f, 0.01f, "%.6f");
                    changed |= ImGui::SliderFloat("PCF Radius", &data.pcfRadius, 0.1f, 4.0f);

                    static const std::array<const char*, 3> resolutionLabels = { "Low - 1024px", "Medium - 2048px", "High - 4096px" };
                    int cascadeQualityIndex = static_cast<int>(m_CSM->GetQuality());

                    if (ImGui::Combo("Resolution", &cascadeQualityIndex, resolutionLabels.data(), resolutionLabels.size()))
                    {
                        auto quality = static_cast<CascadedQuality>(cascadeQualityIndex);
                        m_CSM->Resize(quality);
                    }

                    ImGui::Separator();
                    ImGui::Text("Shadow Debug");
                    ImGui::RadioButton("Off##ShadowDbg", &m_Camera.controls.debugShadowMode, 0); ImGui::SameLine();
                    ImGui::RadioButton("Cascades", &m_Camera.controls.debugShadowMode, 1); ImGui::SameLine();
                    ImGui::RadioButton("Visibility", &m_Camera.controls.debugShadowMode, 2);
                    if (changed) m_CSM->Upload();
                }

                ImGui::TreePop();
            }

            // ============ Post Processing Settings ============
            if (ImGui::TreeNodeEx("Post Processing", treeFlags))
            {
                ImGui::SeparatorText("DOF");
                ImGui::Checkbox("Enable DOF", &m_Camera.lens.enableDOF);
                ImGui::SliderFloat("Focal Length", &m_Camera.lens.focalLength, 10.0f, 200.0f);
                ImGui::SliderFloat("FStop", &m_Camera.lens.fStop, 0.7f, 16.0f);
                ImGui::SliderFloat("Focus Range", &m_Camera.lens.focusRange, 0.7f, 16.0f);
                ImGui::SliderFloat("Blur Amount", &m_Camera.lens.blurAmount, 0.5f, 20.0f);

                ImGui::SeparatorText("Vignette");
                ImGui::Checkbox("Enable Vignette", &m_Camera.postProcessing.enableVignette);
                ImGui::SliderFloat("Vignette Radius", &m_Camera.postProcessing.vignetteRadius, 0.1f, 1.2f);
                ImGui::SliderFloat("Vignette Softness", &m_Camera.postProcessing.vignetteSoftness, 0.001f, 1.0f);
                ImGui::SliderFloat("Vignette Intensity", &m_Camera.postProcessing.vignetteIntensity, 0.0f, 2.0f);
                ImGui::ColorEdit3("Vignette Color", &m_Camera.postProcessing.vignetteColor.x);

                ImGui::SeparatorText("Chromatic Aberation");
                ImGui::Checkbox("Enable Chromatic Aberation", &m_Camera.postProcessing.enableChromAb);
                ImGui::SliderFloat("Amount", &m_Camera.postProcessing.chromAbAmount, 0.0f, 0.03f, "%.4f");
                ImGui::SliderFloat("Radial", &m_Camera.postProcessing.chromAbRadial, 0.1f, 3.0f);

                ImGui::SeparatorText("Bloom");
                ImGui::Checkbox("Enable Bloom", &m_Camera.postProcessing.enableBloom);
                ImGui::DragFloat("Threshold", &m_Bloom->settings.threshold, 0.025, 0.0f, FLT_MAX);
                ImGui::DragFloat("Intensity", &m_Bloom->settings.intensity, 0.025, 0.0f, FLT_MAX);
                ImGui::DragFloat("Knee", &m_Bloom->settings.knee, 0.25, 0.0f, FLT_MAX);
                ImGui::DragFloat("Radius", &m_Bloom->settings.radius, 0.025, 0.0f, 1.0f);
                ImGui::SliderInt("Iterations", &m_Bloom->settings.iterations, 1, 8);

                ImGui::SeparatorText("SSAO");
                ImGui::Checkbox("Enable SSAO", &m_Camera.postProcessing.enableSSAO);
                ImGui::Checkbox("Debug SSAO", &m_Camera.postProcessing.debugSSAO);
                ImGui::DragFloat("AO Radius", &m_Camera.postProcessing.aoRadius, 0.01f, 0.05f, 5.0f);
                ImGui::DragFloat("AO Bias", &m_Camera.postProcessing.aoBias, 0.001f, 0.0f, 0.2f, "%.4f");
                ImGui::DragFloat("AO Intensity", &m_Camera.postProcessing.aoIntensity, 0.01f, 0.0f, 4.0f);
                ImGui::DragFloat("AO Power", &m_Camera.postProcessing.aoPower, 0.01f, 0.1f, 4.0f);

                ImGui::TreePop();
            }

            // ============ Render Mode ============
            if (ImGui::TreeNodeEx("Render Mode", treeFlags))
            {
                int mode = static_cast<int>(m_SceneData.renderMode);
                if (ImGui::RadioButton("Color", mode == RENDER_MODE_COLOR)) mode = RENDER_MODE_COLOR;
                if (ImGui::RadioButton("Normals", mode == RENDER_MODE_NORMALS)) mode = RENDER_MODE_NORMALS;
                if (ImGui::RadioButton("Metallic", mode == RENDER_MODE_METALLIC)) mode = RENDER_MODE_METALLIC;
                if (ImGui::RadioButton("Roughness", mode == RENDER_MODE_ROUGHNESS)) mode = RENDER_MODE_ROUGHNESS;
                if (ImGui::RadioButton("Depth", mode == RENDER_MODE_DEPTH)) mode = RENDER_MODE_DEPTH;
                m_SceneData.renderMode = static_cast<float>(mode);

                ImGui::TreePop();
            }
        }

        ImGui::End();
    }

    void App::UISceneHierarchy()
    {
        if (ImGui::Begin("Scene", nullptr))
        {
            for (size_t i = 0; i < m_Scene.models.size(); ++i)
            {
                ImGui::PushID(i);
                const auto& model = m_Scene.models[i];
                std::stringstream ss;
                ss << "Model " << static_cast<int>(i);
                if (ImGui::CollapsingHeader(ss.str().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    bool removing = false;
                    if (ImGui::Button("Remove"))
                    {
                        removing = m_Scene.RemoveModel(static_cast<int>(i));
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
    }

    void App::UISceneProperties()
    {
        ImGui::Begin("Properties", nullptr);

        ImGui::End();
    }

    void App::OnMouseScroll(float xoffset, float yoffset)
    {
        if (m_Vp.isHovered)
        {
            m_Camera.HandleZoom(yoffset);
        }
    }

    void App::OnMouseMotion(const glm::vec2& position, const glm::vec2& delta)
    {
        if (m_Vp.isHovered)
        {
            m_Camera.HandleOrbit(delta);
            m_Camera.HandlePan(delta);
        }
    }
}