// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "App.h"
#include "Physics/JoltPhysics.h"
#include "Scene/Components.h"
#include "Renderer/Material.h"
#include "Renderer/Renderer2D.h"
#include "Math/Math.hpp"

#include "SDL3/SDL_dialog.h"
#include "SDL3/SDL_events.h"

#include <ImGuizmo.h>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <iterator>
#include <cstring>
#include <cstdint>
#include <cfloat>

namespace flex
{
    namespace
    {
        const SDL_DialogFileFilter kSceneFileFilters[] =
        {
            { "Flex Scene", "json" },
            { "All Files", "*" }
        };

        constexpr const char* kHierarchyEntityPayload = "FLEX_ENTITY_HIERARCHY";
    }

    App::App(int argc, char** argv)
    {
        WindowCreateInfo windowCI;
        windowCI.fullscreen = false;
        windowCI.title = "Flex Engine - OpenGL 4.6 Renderer";
        windowCI.width = 1280;
        windowCI.height = 720;
        m_Window = CreateRef<Window>(windowCI);

        // Initialize Window Callbacks
        m_Window->SetKeyboardCallback([this](SDL_Keycode key, SDL_Scancode scancode, SDL_EventType type, SDL_Keymod mod) { App::OnKeyPressed(key, scancode, type, mod); });
        m_Window->SetMouseMotionCallback([this](const glm::vec2 &position, const glm::vec2 &delta) { App::OnMouseMotion(position, delta); });
        m_Window->SetScrollCallback([this](float x, float y) { App::OnMouseScroll(x, y); });

        // Initialize Renderer
        Renderer::Init();
        Renderer2D::Init();

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

        JoltPhysics::Init();
        m_Screen = CreateRef<Screen>();

        m_EditorScene = CreateRef<Scene>();
        m_ActiveScene = m_EditorScene;
    }

    App::~App()
    {
        m_ActiveScene.reset();
        m_EditorScene.reset();

        MeshLoader::ClearCache();

        JoltPhysics::Shutdown();
        ImGuiContext::Shutdown();
        TextRenderer::Shutdown();
        Renderer2D::Shutdown();
        Renderer::Shutdown();
    }

    void App::Run()
    {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        Ref<Shader> PBRShader = Renderer::CreateShaderFromFile(
            {
                ShaderData{"Resources/shaders/pbr.vert.glsl", GL_VERTEX_SHADER, 0 },
                ShaderData{"Resources/shaders/pbr.frag.glsl", GL_FRAGMENT_SHADER, 0 },
			}, "MaterialPBR");

        // Shadow depth shader (cascaded)
        Ref<Shader> shadowDepthShader = Renderer::CreateShaderFromFile(
            {
                ShaderData{"Resources/shaders/shadow_depth.vert.glsl", GL_VERTEX_SHADER, 0 },
                ShaderData{"Resources/shaders/shadow_depth.frag.glsl", GL_FRAGMENT_SHADER, 0 },
            }, "ShadowDepth");

        Ref<Shader> skyboxShader = Renderer::CreateShaderFromFile(
			{
				ShaderData{"Resources/shaders/skybox.vert.glsl", GL_VERTEX_SHADER, 0 },
				ShaderData{"Resources/shaders/skybox.frag.glsl", GL_FRAGMENT_SHADER, 0 },
			}, "SkyBox");

        TextureCreateInfo createInfo;
        createInfo.flip = false;
        createInfo.format = Format::RGB32F;
        createInfo.clampMode = WrapMode::REPEAT;
        createInfo.filter = FilterMode::LINEAR;

        m_EnvMap = CreateRef<Texture2D>(createInfo, "Resources/hdr/rogland_clear_night_4k.hdr");

        createInfo.format = Format::RGBA8;
        m_FallbackTexture = CreateRef<Texture2D>(createInfo, "Resources/textures/fallback.jpg");

        // Create skybox mesh
        auto skyboxMesh = MeshLoader::CreateSkyboxCube();
        
        CameraBuffer cameraData{};
        m_CSM = CreateRef<CascadedShadowMap>(CascadedQuality::Medium); // uses binding = 3 for UBO

        Ref<UniformBuffer> cameraUbo = UniformBuffer::Create(sizeof(CameraBuffer), UNIFORM_BINDING_LOC_CAMERA);
        Ref<UniformBuffer> sceneUbo = UniformBuffer::Create(sizeof(SceneData), UNIFORM_BINDING_LOC_SCENE);

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
        if (m_ActiveScene)
        {
            m_ActiveScene->ResizeViewport(GetSceneViewportSize());
        }

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

            ProcessPendingSceneActions();
            ProcessPendingMeshImports();

            const uint64_t currentCount = SDL_GetPerformanceCounter();
            m_FrameData.deltaTime = static_cast<float>(currentCount - prevCount) / freq;
            prevCount = currentCount;
            m_FrameData.fps = 1.0f / m_FrameData.deltaTime;

            if (m_ActiveScene)
            {
                m_ActiveScene->Update(m_FrameData.deltaTime);
            }

            const float aspect = static_cast<float>(m_Vp.viewport.width) / static_cast<float>(m_Vp.viewport.height);
            const bool usingRuntimeCamera = ApplyRuntimeCamera();
            if (!usingRuntimeCamera)
            {
                m_Camera.OnUpdate(m_FrameData.deltaTime);
                m_Camera.UpdateMatrices(aspect > 0.0f ? aspect : 16.0f / 9.0f);
                m_Camera.lens.focalDistance = m_Camera.distance;
            }

            m_Screen->inverseProjection = glm::inverse(m_Camera.projection);
            cameraData.viewProjection = m_Camera.projection * m_Camera.view;
            cameraData.position = glm::vec4(m_Camera.position, 1.0f);
            cameraData.view = m_Camera.view; // new field used by shadows (also u_View uniform separately)
            cameraUbo->SetData(&cameraData, sizeof(cameraData));

            Renderer2D::BeginBatch(cameraData.viewProjection);

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
                shadowDepthShader->Use();
                shadowDepthShader->SetUniform("u_CascadeIndex", ci);
                m_ActiveScene->RenderDepth(shadowDepthShader);
            }
            m_CSM->EndCascade();
            glCullFace(GL_BACK);

            // FIRST PASS: Render to framebuffer
            m_SceneFB->Bind(m_Vp.viewport);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

            // Render models first
            glCullFace(GL_BACK);
            PBRShader->Use();
            // Bind cascaded shadow map (binding = 6 in pbr.frag)
            m_CSM->BindTexture(6);
            PBRShader->SetUniform("u_ShadowMap", 6);
            PBRShader->SetUniform("u_DebugShadows", m_Camera.controls.debugShadowMode);

            m_ActiveScene->Render(PBRShader, m_EnvMap);

            if (m_ActiveScene)
            {
                m_ActiveScene->DebugDrawColliders();
            }

            Renderer2D::EndBatch();

            // Only render on perspective mode
            if (m_Camera.projectionType == ProjectionType::Perspective)
            {
                // Render skybox last (no depth writes, pass when depth equals far plane)
                glDepthMask(GL_FALSE);
                GLint prevDepthFunc; glGetIntegerv(GL_DEPTH_FUNC, &prevDepthFunc);
                glDepthFunc(GL_LEQUAL);

                glCullFace(GL_FRONT);
				skyboxShader->Use();
                // Create skybox transformation (remove translation from view)
                auto skyboxView = glm::mat4(glm::mat3(m_Camera.view));
                auto skyboxMVP = m_Camera.projection * skyboxView;
                skyboxShader->SetUniform("u_Transform", skyboxMVP);
                m_EnvMap->Bind(0);
                skyboxShader->SetUniform("u_EnvironmentMap", 0);
                skyboxMesh->mesh->vertexArray->Bind();
                Renderer::DrawIndexed(skyboxMesh->mesh->vertexArray);

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

    void App::OnScenePlay()
    {
        if (!m_ActiveScene || m_ActiveScene->IsPlaying())
        {
            return;
        }

        if (!m_EditorScene)
        {
            m_EditorScene = m_ActiveScene ? m_ActiveScene : CreateRef<Scene>();
        }

        if (!m_HasCameraBackup)
        {
            m_EditorCameraBackup = m_Camera;
            m_HasCameraBackup = true;
        }

        uint64_t selectedUUIDValue = 0;
        bool hasSelection = false;
        if (m_SelectedEntity != entt::null && m_EditorScene->IsValid(m_SelectedEntity) && m_EditorScene->HasComponent<TagComponent>(m_SelectedEntity))
        {
            selectedUUIDValue = static_cast<uint64_t>(m_EditorScene->GetComponent<TagComponent>(m_SelectedEntity).uuid);
            hasSelection = true;
        }

        Ref<Scene> runtimeScene = m_EditorScene->Clone();
        m_ActiveScene = runtimeScene;
        m_ActiveScene->Start();
        m_ActiveScene->ResizeViewport(GetSceneViewportSize());
        
#if 0
        entt::entity player = m_ActiveScene->GetEntityByName("player");
        if (player != entt::null)
        {
            auto &rb = m_ActiveScene->GetComponent<RigidbodyComponent>(player);
            rb.onContactEnter = [&](const PhysicsContactData& data) -> void
            {
                const std::string &name = m_ActiveScene->GetEntityName(data.otherEntity);
                SDL_Log("%s\n", name.c_str());
            };
        }
#endif
        if (hasSelection)
        {
            m_SelectedEntity = m_ActiveScene->GetEntityByUUID(UUID(selectedUUIDValue));
        }
        else
        {
            m_SelectedEntity = entt::null;
        }
    }

    void App::OnSceneStop()
    {
        if (!m_ActiveScene || !m_ActiveScene->IsPlaying())
        {
            return;
        }

        uint64_t selectedUUIDValue = 0;
        bool hasSelection = false;
        if (m_SelectedEntity != entt::null && m_ActiveScene->IsValid(m_SelectedEntity) && m_ActiveScene->HasComponent<TagComponent>(m_SelectedEntity))
        {
            selectedUUIDValue = static_cast<uint64_t>(m_ActiveScene->GetComponent<TagComponent>(m_SelectedEntity).uuid);
            hasSelection = true;
        }

        m_ActiveScene->Stop();
        m_ActiveScene = m_EditorScene;
        if (m_ActiveScene)
        {
            m_ActiveScene->ResizeViewport(GetSceneViewportSize());
        }

        if (m_HasCameraBackup)
        {
            m_Camera = m_EditorCameraBackup;
            m_HasCameraBackup = false;
        }

        if (hasSelection && m_ActiveScene)
        {
            m_SelectedEntity = m_ActiveScene->GetEntityByUUID(UUID(selectedUUIDValue));
        }
        else
        {
            m_SelectedEntity = entt::null;
        }
    }

     void App::DrawHierarchyNode(entt::entity entity)
    {
        if (!m_ActiveScene || !m_ActiveScene->IsValid(entity))
        {
            return;
        }

        TagComponent& tag = m_ActiveScene->GetComponent<TagComponent>(entity);
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
        if (m_SelectedEntity == entity)
        {
            flags |= ImGuiTreeNodeFlags_Selected;
        }
        if (tag.children.empty())
        {
            flags |= ImGuiTreeNodeFlags_Leaf;
        }

        const std::string label = tag.name.empty()
            ? std::format("Entity - {}", static_cast<uint64_t>(tag.uuid)) : tag.name;
        const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<uintptr_t>(entity)), flags, "%s", label.c_str());

        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
        {
            m_SelectedEntity = entity;
        }

        if (ImGui::BeginDragDropSource())
        {
            const uint64_t uuidValue = static_cast<uint64_t>(tag.uuid);
            ImGui::SetDragDropPayload(kHierarchyEntityPayload, &uuidValue, sizeof(uuidValue));
            ImGui::TextUnformatted(tag.name.empty() ? "Entity" : tag.name.c_str());
            ImGui::EndDragDropSource();
        }

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kHierarchyEntityPayload))
            {
                if (payload->DataSize == sizeof(uint64_t))
                {
                    const uint64_t droppedUUID = *static_cast<const uint64_t*>(payload->Data);
                    entt::entity droppedEntity = m_ActiveScene->GetEntityByUUID(UUID(droppedUUID));
                    if (droppedEntity != entt::null)
                    {
                        m_ActiveScene->ReparentEntity(droppedEntity, entity);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        DrawEntityContextMenu(entity);

        if (opened)
        {
            std::vector<entt::entity> childEntities;
            childEntities.reserve(tag.children.size());
            for (const UUID& childUUID : tag.children)
            {
                entt::entity childEntity = m_ActiveScene->GetEntityByUUID(childUUID);
                if (childEntity != entt::null)
                {
                    childEntities.push_back(childEntity);
                }
            }

            auto sortByName = [&](entt::entity lhs, entt::entity rhs)
            {
                const std::string& lhsName = m_ActiveScene->GetComponent<TagComponent>(lhs).name;
                const std::string& rhsName = m_ActiveScene->GetComponent<TagComponent>(rhs).name;
                return lhsName < rhsName;
            };
            std::sort(childEntities.begin(), childEntities.end(), sortByName);

            for (entt::entity childEntity : childEntities)
            {
                DrawHierarchyNode(childEntity);
            }

            ImGui::TreePop();
        }
    }

    void App::DrawEntityContextMenu(entt::entity entity)
    {
        if (!m_ActiveScene || !m_ActiveScene->IsValid(entity))
        {
            return;
        }

        const TagComponent& tag = m_ActiveScene->GetComponent<TagComponent>(entity);
        const std::string popupId = std::format("EntityContext{}", static_cast<uint64_t>(tag.uuid));
        if (ImGui::BeginPopupContextItem(popupId.c_str()))
        {
            if (ImGui::MenuItem("Create Empty Child"))
            {
                entt::entity child = CreateEmptyEntity("Entity", entity);
                if (child != entt::null)
                {
                    m_SelectedEntity = child;
                }
            }

            const bool hasParent = m_ActiveScene->GetParentEntity(entity) != entt::null;
            if (ImGui::MenuItem("Unparent", nullptr, false, hasParent))
            {
                m_ActiveScene->ReparentEntity(entity, entt::null);
            }

            if (ImGui::MenuItem("Duplicate"))
            {
                entt::entity duplicate = m_ActiveScene->DuplicateEntity(entity);
                if (duplicate != entt::null)
                {
                    const glm::mat4 world = m_ActiveScene->GetWorldTransform(entity);
                    m_ActiveScene->SetWorldTransform(duplicate, world);
                    m_SelectedEntity = duplicate;
                }
            }

            if (ImGui::MenuItem("Delete"))
            {
                m_PendingHierarchyDeletion.push_back(entity);
            }

            if (m_ActiveScene->HasComponent<CameraComponent>(entity))
            {
                const bool isPrimary = m_ActiveScene->GetPrimaryCamera() == entity;
                if (ImGui::MenuItem("Set As Primary Camera", nullptr, false, !isPrimary))
                {
                    m_ActiveScene->SetPrimaryCamera(entity);
                }
            }

            ImGui::EndPopup();
        }
    }

    void App::HandleHierarchyWindowContextMenu()
    {
        if (!m_ActiveScene)
        {
            return;
        }

        if (ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Create Empty Entity"))
            {
                entt::entity entity = CreateEmptyEntity("Entity");
                m_SelectedEntity = entity;
            }

            const bool hasSelection = m_SelectedEntity != entt::null && m_ActiveScene->IsValid(m_SelectedEntity);
            if (ImGui::MenuItem("Create Child Of Selection", nullptr, false, hasSelection))
            {
                entt::entity entity = CreateEmptyEntity("Entity", m_SelectedEntity);
                m_SelectedEntity = entity;
            }

            if (ImGui::MenuItem("Unparent Selection", nullptr, false, hasSelection))
            {
                m_ActiveScene->ReparentEntity(m_SelectedEntity, entt::null);
            }

            ImGui::EndPopup();
        }
    }

    entt::entity App::CreateEmptyEntity(const std::string& baseName, entt::entity parent)
    {
        if (!m_ActiveScene)
        {
            return entt::null;
        }

        const std::string uniqueName = GenerateUniqueEntityName(baseName);
        entt::entity entity = m_ActiveScene->CreateEntity(uniqueName);
        m_ActiveScene->AddComponent<TransformComponent>(entity);
        if (parent != entt::null)
        {
            m_ActiveScene->ReparentEntity(entity, parent);
        }
        return entity;
    }

    std::string App::GenerateUniqueEntityName(const std::string& baseName) const
    {
        if (!m_ActiveScene)
        {
            return baseName;
        }

        auto nameExists = [&](const std::string& candidate)
        {
            for (const auto& [uuid, entity] : m_ActiveScene->entities)
            {
                if (!m_ActiveScene->HasComponent<TagComponent>(entity))
                {
                    continue;
                }
                if (m_ActiveScene->GetComponent<TagComponent>(entity).name == candidate)
                {
                    return true;
                }
            }
            return false;
        };

        std::string uniqueName = baseName;
        int suffix = 1;
        while (nameExists(uniqueName))
        {
            uniqueName = std::format("{} ({})", baseName, suffix++);
        }
        return uniqueName;
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
            const char *playStopStr = m_ActiveScene->IsPlaying() ? "Stop" : "Play";
            if (ImGui::Button(playStopStr))
            {
                if (m_ActiveScene->IsPlaying())
                {
                    OnSceneStop();
                }
                else
                {
                    OnScenePlay();
                }
            }

            ImGui::SameLine();
            ImGui::TextUnformatted("Operation");
            ImGui::SameLine();
            static const char* kGizmoOperationLabels[] = { "Translate", "Rotate", "Scale" };
            int operationIndex = 0;
            switch (m_GizmoOperation)
            {
            case ImGuizmo::ROTATE: operationIndex = 1; break;
            case ImGuizmo::SCALE: operationIndex = 2; break;
            default: operationIndex = 0; break;
            }
            ImGui::SetNextItemWidth(140.0f);
            if (ImGui::Combo("##GizmoOperation", &operationIndex, kGizmoOperationLabels, IM_ARRAYSIZE(kGizmoOperationLabels)))
            {
                m_GizmoOperation = operationIndex == 0 ? ImGuizmo::TRANSLATE : operationIndex == 1 ? ImGuizmo::ROTATE : ImGuizmo::SCALE;
            }
            ImGui::SameLine();
            ImGui::TextUnformatted("Mode");
            ImGui::SameLine();
            static const char* kGizmoModeLabels[] = { "Local", "World" };
            int modeIndex = m_GizmoMode == ImGuizmo::LOCAL ? 0 : 1;
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::Combo("##GizmoMode", &modeIndex, kGizmoModeLabels, IM_ARRAYSIZE(kGizmoModeLabels)))
            {
                m_GizmoMode = modeIndex == 0 ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
            }

            ImGui::SameLine();
            ImGui::TextUnformatted("Aspect");
            ImGui::SameLine();
            static const char* kAspectModeLabels[] = { "Free", "Fixed" };
            int aspectModeIndex = m_ViewportOptions.fixedAspect ? 1 : 0;
            ImGui::SetNextItemWidth(100.0f);
            if (ImGui::Combo("##AspectMode", &aspectModeIndex, kAspectModeLabels, IM_ARRAYSIZE(kAspectModeLabels)))
            {
                m_ViewportOptions.fixedAspect = aspectModeIndex == 1;
            }
            if (m_ViewportOptions.fixedAspect)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(80.0f);
                if (ImGui::InputFloat("##AspectValue", &m_ViewportOptions.aspectRatio, 0.0f, 0.0f, "%.2f"))
                {
                    m_ViewportOptions.aspectRatio = std::max(m_ViewportOptions.aspectRatio, 0.1f);
                }
                ImGui::SameLine();
                if (ImGui::Button("16:9"))
                {
                    m_ViewportOptions.aspectRatio = 16.0f / 9.0f;
                }
                ImGui::SameLine();
                if (ImGui::Button("4:3"))
                {
                    m_ViewportOptions.aspectRatio = 4.0f / 3.0f;
                }
            }

            const ImVec2 cursorStart = ImGui::GetCursorPos();
            ImVec2 viewportAvail = ImGui::GetContentRegionAvail();
            ImVec2 renderSize = viewportAvail;
            ImVec2 renderPos = cursorStart;

            if (m_ViewportOptions.fixedAspect && viewportAvail.x > 0.0f && viewportAvail.y > 0.0f)
            {
                const float targetAspect = std::max(m_ViewportOptions.aspectRatio, 0.01f);
                const float availAspect = viewportAvail.x / viewportAvail.y;

                if (availAspect > targetAspect)
                {
                    renderSize.x = viewportAvail.y * targetAspect;
                    renderSize.y = viewportAvail.y;
                }
                else
                {
                    renderSize.x = viewportAvail.x;
                    renderSize.y = viewportAvail.x / targetAspect;
                }

                renderPos.x += (viewportAvail.x - renderSize.x) * 0.5f;
                renderPos.y += (viewportAvail.y - renderSize.y) * 0.5f;
            }

            ImVec2 cursorAfterImage(cursorStart.x, cursorStart.y + viewportAvail.y);

            const float clampedWidth = std::max(renderSize.x, 1.0f);
            const float clampedHeight = std::max(renderSize.y, 1.0f);
            const uint32_t newViewportWidth = static_cast<uint32_t>(clampedWidth);
            const uint32_t newViewportHeight = static_cast<uint32_t>(clampedHeight);

            if (newViewportWidth != m_Vp.viewport.width || newViewportHeight != m_Vp.viewport.height)
            {
                m_Vp.viewport.width = newViewportWidth;
                m_Vp.viewport.height = newViewportHeight;
                if (m_ActiveScene)
                {
                    m_ActiveScene->ResizeViewport(glm::vec2(clampedWidth, clampedHeight));
                }
            }

            // Display framebuffer color attachment as image
            const uint32_t colorTex = m_ViewportFB->GetColorAttachment(0);
            if (colorTex != 0 && renderSize.x > 0.0f && renderSize.y > 0.0f)
            {
                ImGuizmo::BeginFrame();
                ImGui::SetCursorPos(renderPos);
                ImGui::Image(colorTex, renderSize, ImVec2(0, 1), ImVec2(1, 0));

                if (m_SelectedEntity != entt::null && m_ActiveScene->HasComponent<TransformComponent>(m_SelectedEntity))
                {
                    glm::mat4 model = m_ActiveScene->GetWorldTransform(m_SelectedEntity);
                    glm::mat4 view = m_Camera.view;
                    glm::mat4 projection = m_Camera.projection;

                    const ImVec2 gizmoMin = ImGui::GetItemRectMin();
                    const ImVec2 gizmoMax = ImGui::GetItemRectMax();
                    ImGuizmo::SetOrthographic(m_Camera.projectionType == ProjectionType::Orthographic);
                    ImGuizmo::SetDrawlist();
                    ImGuizmo::SetRect(gizmoMin.x, gizmoMin.y, gizmoMax.x - gizmoMin.x, gizmoMax.y - gizmoMin.y);

                    if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), m_GizmoOperation, m_GizmoMode, glm::value_ptr(model)))
                    {
                        m_ActiveScene->SetWorldTransform(m_SelectedEntity, model);
                    }
                }
            }

            // ImGui::SetCursorPos(cursorAfterImage);
        }
        m_Vp.isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
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
                ImGui::DragFloat("Fog Density", &m_SceneData.fogDensity, 0.0f, 0.0f, 1000.0f);
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

                ImGui::SeparatorText("Chromatic Aberration");
                ImGui::Checkbox("Enable Chromatic Aberration", &m_Camera.postProcessing.enableChromAb);
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
        if (!m_ActiveScene)
        {
            return;
        }

        if (ImGui::Begin("Hierarchy", nullptr))
        {
            std::vector<entt::entity> roots;
            roots.reserve(m_ActiveScene->entities.size());
            for (const auto& [uuid, entity] : m_ActiveScene->entities)
            {
                if (m_ActiveScene->GetParentEntity(entity) == entt::null)
                {
                    roots.push_back(entity);
                }
            }

            auto sortByName = [&](entt::entity lhs, entt::entity rhs)
            {
                const bool valid = m_ActiveScene->IsValid(lhs) && m_ActiveScene->IsValid(rhs);
                if (!valid)
                    return false;
                    
                const std::string& lhsName = m_ActiveScene->GetComponent<TagComponent>(lhs).name;
                const std::string& rhsName = m_ActiveScene->GetComponent<TagComponent>(rhs).name;
                return lhsName < rhsName;
            };

            if (m_PendingHierarchyDeletion.empty())
            {
                std::sort(roots.begin(), roots.end(), sortByName);
            }

            for (entt::entity entity : roots)
            {
                DrawHierarchyNode(entity);
            }

            HandleHierarchyWindowContextMenu();
        }
        ImGui::End();

        if (!m_PendingHierarchyDeletion.empty())
        {
            for (entt::entity entity : m_PendingHierarchyDeletion)
            {
                if (!m_ActiveScene->IsValid(entity))
                {
                    continue;
                }

                if (m_SelectedEntity == entity)
                {
                    m_SelectedEntity = entt::null;
                }

                m_ActiveScene->DestroyEntity(entity);
            }
            m_PendingHierarchyDeletion.clear();
        }
    }

    template<typename T>
    void App::DrawComponentHeader(const char* label, entt::entity entity, ImGuiTreeNodeFlags flags, bool& opened)
    {
        opened = ImGui::TreeNodeEx(label, flags);
        
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 20.0f);
        ImGui::PushID(label);
        if (ImGui::Button("...", ImVec2(24.0f, 0.0f)))
        {
            ImGui::OpenPopup("ComponentContextMenu");
        }
        
        if (ImGui::BeginPopup("ComponentContextMenu"))
        {
            if (ImGui::MenuItem("Copy"))
            {
                if constexpr (std::is_same_v<T, TransformComponent>)
                {
                    m_CopiedTransform = m_ActiveScene->GetComponent<T>(entity);
                }
                else if constexpr (std::is_same_v<T, CameraComponent>)
                {
                    m_CopiedCamera = m_ActiveScene->GetComponent<T>(entity);
                }
                else if constexpr (std::is_same_v<T, MeshComponent>)
                {
                    m_CopiedMesh = m_ActiveScene->GetComponent<T>(entity);
                }
                else if constexpr (std::is_same_v<T, RigidbodyComponent>)
                {
                    m_CopiedRigidbody = m_ActiveScene->GetComponent<T>(entity);
                }
                else if constexpr (std::is_same_v<T, BoxColliderComponent>)
                {
                    m_CopiedBoxCollider = m_ActiveScene->GetComponent<T>(entity);
                }
                else if constexpr (std::is_same_v<T, CapsuleColliderComponent>)
                {
                    m_CopiedCapsuleCollider = m_ActiveScene->GetComponent<T>(entity);
                }
                else if constexpr (std::is_same_v<T, SphereColliderComponent>)
                {
                    m_CopiedSphereCollider = m_ActiveScene->GetComponent<T>(entity);
                }
                else if constexpr (std::is_same_v<T, PlaneColliderComponent>)
                {
                    m_CopiedPlaneCollider = m_ActiveScene->GetComponent<T>(entity);
                }
            }
            
            bool canPaste = false;
            if constexpr (std::is_same_v<T, TransformComponent>)
                canPaste = m_CopiedTransform.has_value();
            else if constexpr (std::is_same_v<T, CameraComponent>)
                canPaste = m_CopiedCamera.has_value();
            else if constexpr (std::is_same_v<T, MeshComponent>)
                canPaste = m_CopiedMesh.has_value();
            else if constexpr (std::is_same_v<T, RigidbodyComponent>)
                canPaste = m_CopiedRigidbody.has_value();
            else if constexpr (std::is_same_v<T, BoxColliderComponent>)
                canPaste = m_CopiedBoxCollider.has_value();
            else if constexpr (std::is_same_v<T, CapsuleColliderComponent>)
                canPaste = m_CopiedCapsuleCollider.has_value();
            else if constexpr (std::is_same_v<T, SphereColliderComponent>)
                canPaste = m_CopiedSphereCollider.has_value();
            else if constexpr (std::is_same_v<T, PlaneColliderComponent>)
                canPaste = m_CopiedPlaneCollider.has_value();
            
            if (!canPaste)
                ImGui::BeginDisabled();
            
            if (ImGui::MenuItem("Paste"))
            {
                if constexpr (std::is_same_v<T, TransformComponent>)
                {
                    if (m_CopiedTransform.has_value())
                        m_ActiveScene->GetComponent<T>(entity) = m_CopiedTransform.value();
                }
                else if constexpr (std::is_same_v<T, CameraComponent>)
                {
                    if (m_CopiedCamera.has_value())
                        m_ActiveScene->GetComponent<T>(entity) = m_CopiedCamera.value();
                }
                else if constexpr (std::is_same_v<T, MeshComponent>)
                {
                    if (m_CopiedMesh.has_value())
                        m_ActiveScene->GetComponent<T>(entity) = m_CopiedMesh.value();
                }
                else if constexpr (std::is_same_v<T, RigidbodyComponent>)
                {
                    if (m_CopiedRigidbody.has_value())
                        m_ActiveScene->GetComponent<T>(entity) = m_CopiedRigidbody.value();
                }
                else if constexpr (std::is_same_v<T, BoxColliderComponent>)
                {
                    if (m_CopiedBoxCollider.has_value())
                        m_ActiveScene->GetComponent<T>(entity) = m_CopiedBoxCollider.value();
                }
                else if constexpr (std::is_same_v<T, CapsuleColliderComponent>)
                {
                    if (m_CopiedCapsuleCollider.has_value())
                        m_ActiveScene->GetComponent<T>(entity) = m_CopiedCapsuleCollider.value();
                }
                else if constexpr (std::is_same_v<T, SphereColliderComponent>)
                {
                    if (m_CopiedSphereCollider.has_value())
                        m_ActiveScene->GetComponent<T>(entity) = m_CopiedSphereCollider.value();
                }
                else if constexpr (std::is_same_v<T, PlaneColliderComponent>)
                {
                    if (m_CopiedPlaneCollider.has_value())
                        m_ActiveScene->GetComponent<T>(entity) = m_CopiedPlaneCollider.value();
                }
            }
            
            if (!canPaste)
                ImGui::EndDisabled();
            
            // Don't allow removing Transform component as it's essential
            if constexpr (!std::is_same_v<T, TransformComponent>)
            {
                ImGui::Separator();
                if (ImGui::MenuItem("Remove"))
                {
                    m_ActiveScene->RemoveComponent<T>(entity);
                }
            }
            
            ImGui::EndPopup();
        }
        
        ImGui::PopID();
    }

    void App::UISceneProperties()
    {
        ImGui::Begin("Properties", nullptr);

        static auto treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_Framed;

        if (m_SelectedEntity != entt::null)
        {
            TagComponent& tag = m_ActiveScene->GetComponent<TagComponent>(m_SelectedEntity);
            static char nameBuffer[256] = { 0 };
            static entt::entity bufferedEntity = entt::null;
            if (bufferedEntity != m_SelectedEntity)
            {
                constexpr std::size_t kBufferSize = sizeof(nameBuffer);
                std::fill(std::begin(nameBuffer), std::end(nameBuffer), '\0');
                const std::size_t copyLength = std::min(tag.name.size(), kBufferSize - 1);
                if (copyLength > 0)
                {
                    std::memcpy(nameBuffer, tag.name.data(), copyLength);
                    nameBuffer[copyLength] = '\0';
                }
                bufferedEntity = m_SelectedEntity;
            }

            if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
            {
                tag.name = nameBuffer[0] ? nameBuffer : "Entity";
            }

            if (m_ActiveScene->HasComponent<TransformComponent>(m_SelectedEntity))
            {
                auto& tr = m_ActiveScene->GetComponent<TransformComponent>(m_SelectedEntity);

                bool opened;
                DrawComponentHeader<TransformComponent>("Transform", m_SelectedEntity, treeNodeFlags, opened);
                if (opened)
                {
                    ImGui::DragFloat3("Position", &tr.position.x, 0.025);
                    ImGui::DragFloat3("Rotation", &tr.rotation.x, 0.025);
                    ImGui::DragFloat3("Scale", &tr.scale.x, 0.025);

                    ImGui::TreePop();
                }
            }

            if (m_ActiveScene->HasComponent<CameraComponent>(m_SelectedEntity))
            {
                auto& cc = m_ActiveScene->GetComponent<CameraComponent>(m_SelectedEntity);
                bool opened;
                DrawComponentHeader<CameraComponent>("Camera", m_SelectedEntity, treeNodeFlags, opened);
                if (opened)
                {
                    bool projectionDirty = false;
                    static const std::array<const char*, 2> projLabels = { "Perspective", "Orthographic" };
                    int projIndex = cc.projectionType == ProjectionType::Perspective ? 0 : 1;
                    if (ImGui::Combo("Projection", &projIndex, projLabels.data(), projLabels.size()))
                    {
                        cc.projectionType = projIndex == 0 ? ProjectionType::Perspective : ProjectionType::Orthographic;
                        projectionDirty = true;
                    }

                    projectionDirty |= ImGui::DragFloat("Near Plane", &cc.nearPlane, 0.025f, 0.0001f, FLT_MAX, "%.3f");
                    projectionDirty |= ImGui::DragFloat("Far Plane", &cc.farPlane, 0.025f, cc.nearPlane + 0.001f, FLT_MAX, "%.3f");

                    if (cc.projectionType == ProjectionType::Perspective)
                    {
                        projectionDirty |= ImGui::SliderFloat("Fov", &cc.fov, 20.0f, 180.0f);
                    }
                    else
                    {
                        projectionDirty |= ImGui::SliderFloat("Ortho Size", &cc.orthoSize, 0.1f, 100.0f);
                    }

                    bool isPrimary = cc.primary;
                    if (ImGui::Checkbox("Primary", &isPrimary))
                    {
                        cc.primary = isPrimary;
                        if (cc.primary)
                        {
                            m_ActiveScene->SetPrimaryCamera(m_SelectedEntity);
                        }
                    }

                    static const char* kCameraAspectModes[] = { "Free", "Fixed" };
                    int cameraAspectIndex = static_cast<int>(cc.aspectMode);
                    if (ImGui::Combo("Aspect Mode", &cameraAspectIndex, kCameraAspectModes, IM_ARRAYSIZE(kCameraAspectModes)))
                    {
                        cc.aspectMode = static_cast<CameraAspectMode>(cameraAspectIndex);
                        projectionDirty = true;
                    }
                    if (cc.aspectMode == CameraAspectMode::Fixed)
                    {
                        if (ImGui::InputFloat("Fixed Aspect", &cc.fixedAspectRatio, 0.0f, 0.0f, "%.2f"))
                        {
                            cc.fixedAspectRatio = std::max(cc.fixedAspectRatio, 0.1f);
                            projectionDirty = true;
                        }
                    }
                    ImGui::Text("Current Aspect: %.2f", cc.aspectRatio);

                    if (projectionDirty)
                    {
                        cc.RecalculateProjection(m_ActiveScene->GetViewportSize());
                    }

                    ImGui::TreePop();
                }
            }

            if (m_ActiveScene->HasComponent<RigidbodyComponent>(m_SelectedEntity))
            {
                auto& rb = m_ActiveScene->GetComponent<RigidbodyComponent>(m_SelectedEntity);
                bool opened;
                DrawComponentHeader<RigidbodyComponent>("Rigidbody", m_SelectedEntity, treeNodeFlags, opened);
                if (opened)
                {
                    ImGui::DragFloat("Mass", &rb.mass, 0.025f);
                    ImGui::DragFloat3("Center Mass", &rb.centerOfMass.x, 0.01f);
                    ImGui::DragFloat("Gravity Factor", &rb.gravityFactor, 0.25f, 0.0f, 100.0f);
                    ImGui::DragFloat("Friction", &rb.friction, 0.1f, 0.0f, 100.0f);
                    ImGui::DragFloat("Static Friction", &rb.staticFriction, 100.0f);
                    ImGui::DragFloat("Restitution", &rb.restitution, 0.1f, 0.0f, 100.0f);

                    ImGui::Checkbox("Is Static", &rb.isStatic);
                    ImGui::Checkbox("Use Gravity", &rb.useGravity);
                    ImGui::Checkbox("Allow Sleeping", &rb.allowSleeping);

                    ImGui::TreePop();
                }
            }

            if (m_ActiveScene->HasComponent<BoxColliderComponent>(m_SelectedEntity))
            {
                auto& box = m_ActiveScene->GetComponent<BoxColliderComponent>(m_SelectedEntity);
                bool opened;
                DrawComponentHeader<BoxColliderComponent>("Box Collider", m_SelectedEntity, treeNodeFlags, opened);
                if (opened)
                {
                    ImGui::DragFloat3("Offset", &box.offset.x, 0.01f);
                    ImGui::DragFloat("Density", &box.density, 0.1f, 0.0f, 100.0f);
                    ImGui::DragFloat3("Scale", &box.scale.x, 0.01f);
                    
                    ImGui::TreePop();
                }
            }

            if (m_ActiveScene->HasComponent<CapsuleColliderComponent>(m_SelectedEntity))
            {
                auto &capsule = m_ActiveScene->GetComponent<CapsuleColliderComponent>(m_SelectedEntity);
                bool opened;
                DrawComponentHeader<CapsuleColliderComponent>("Capsule Collider", m_SelectedEntity, treeNodeFlags, opened);
                if (opened)
                {
                    ImGui::DragFloat3("Offset", &capsule.offset.x, 0.01f);
                    ImGui::DragFloat("Density", &capsule.density, 0.1f, 0.0f, 100.0f);
                    ImGui::DragFloat("Radius", &capsule.radius, 0.01f);
                    ImGui::DragFloat("Height", &capsule.height, 0.01f);

                    ImGui::TreePop();
                }
            }

            if (m_ActiveScene->HasComponent<SphereColliderComponent>(m_SelectedEntity))
            {
                auto &sphere = m_ActiveScene->GetComponent<SphereColliderComponent>(m_SelectedEntity);
                bool opened;
                DrawComponentHeader<SphereColliderComponent>("Sphere Collider", m_SelectedEntity, treeNodeFlags, opened);
                if (opened)
                {
                    ImGui::DragFloat3("Offset", &sphere.offset.x, 0.01f);
                    ImGui::DragFloat("Density", &sphere.density, 0.1f, 0.0f, 100.0f);
                    ImGui::DragFloat("Radius", &sphere.radius, 0.01f);

                    ImGui::TreePop();
                }
            }

            if (m_ActiveScene->HasComponent<PlaneColliderComponent>(m_SelectedEntity))
            {
                auto &plane = m_ActiveScene->GetComponent<PlaneColliderComponent>(m_SelectedEntity);
                bool opened;
                DrawComponentHeader<PlaneColliderComponent>("Plane Collider", m_SelectedEntity, treeNodeFlags, opened);
                if (opened)
                {
                    ImGui::DragFloat3("Offset", &plane.offset.x, 0.01f);

                    ImGui::TreePop();
                }
            }

            if (m_ActiveScene->HasComponent<MeshComponent>(m_SelectedEntity))
            {
                auto& mc = m_ActiveScene->GetComponent<MeshComponent>(m_SelectedEntity);
                bool opened;
                DrawComponentHeader<MeshComponent>("Mesh", m_SelectedEntity, treeNodeFlags, opened);
                if (opened)
                {
                    if (ImGui::Button("Load Mesh"))
                    {
                        SDL_Log("Opening file dialog...");

                        SDL_DialogFileFilter filters[] =
                        {
                            { "3D Model Files", "gltf;glb" },
                            { "All Files", "*" }
                        };

                        SDL_ShowOpenFileDialog(
                            OnMeshFileSelected,
                            this,
                            m_Window->GetHandle(),
                            filters,
                            std::size(filters),
                            nullptr,
                            false
                        );

                        SDL_Log("SDL_ShowOpenFileDialog called");
                    }

                    if (!mc.meshPath.empty())
                    {
                        ImGui::Text("Mesh: %s", mc.meshPath.c_str());
                    }
                    else
                    {
                        ImGui::Text("No mesh assigned");
                    }

                    if (!m_PendingMeshFilepath.empty())
                    {
                        ImGui::Separator();
                        ImGui::Text("Last imported: %s", m_PendingMeshFilepath.c_str());
                    }

                    if (mc.meshInstance && mc.meshInstance->material)
                    {
                        Ref<Material> material = mc.meshInstance->material;
                        ImGui::SeparatorText("Material");

                        if (!material->name.empty())
                        {
                            ImGui::Text("Name: %s", material->name.c_str());
                        }

                        static const char* kMaterialTypeLabels[] = { "Opaque", "Transparent" };
                        int materialTypeIndex = material->type == MaterialType::Opaque ? 0 : 1;
                        if (ImGui::Combo("Type", &materialTypeIndex, kMaterialTypeLabels, IM_ARRAYSIZE(kMaterialTypeLabels)))
                        {
                            material->type = materialTypeIndex == 0 ? MaterialType::Opaque : MaterialType::Transparent;
                        }

                        ImGui::ColorEdit4("Base Color", &material->params.baseColorFactor.x);
                        ImGui::ColorEdit3("Emissive", &material->params.emissiveFactor.x);
                        ImGui::SliderFloat("Metallic", &material->params.metallicFactor, 0.0f, 1.0f);
                        ImGui::SliderFloat("Roughness", &material->params.roughnessFactor, 0.0f, 1.0f);
                        ImGui::SliderFloat("Occlusion", &material->params.occlusionStrength, 0.0f, 1.0f);

                        ImGui::SeparatorText("Textures");
                        auto drawTexturePreview = [this](const char* label, const Ref<Texture2D>& texture)
                        {
                            ImGui::PushID(label);
                            
                            const Ref<Texture2D>& previewTexture = texture ? texture : m_FallbackTexture;
                            if (previewTexture)
                            {
                                const ImTextureID textureID = static_cast<ImTextureID>(previewTexture->GetHandle());
                                ImGui::Image(textureID, ImVec2(64.0f, 64.0f), ImVec2(0, 1), ImVec2(1, 0));
                                ImGui::NewLine();
                            }
                            else
                            {
                                ImGui::Dummy(ImVec2(64.0f, 64.0f));
                                ImGui::NewLine();
                            }
                            ImGui::SameLine();
                            ImGui::Text("%s", label);
                            
                            ImGui::PopID();
                        };

                        if (ImGui::TreeNodeEx("Textures"))
                        {
                            drawTexturePreview("Base Color", material->baseColorTexture);
                            drawTexturePreview("Emissive", material->emissiveTexture);
                            drawTexturePreview("Metallic/Roughness", material->metallicRoughnessTexture);
                            drawTexturePreview("Normal", material->normalTexture);
                            drawTexturePreview("Occlusion", material->occlusionTexture);
                        }

                    }

                    ImGui::TreePop();
                }
            }

            if (ImGui::Button("Add Component", ImVec2(ImGui::GetContentRegionAvail().x, 24.0f)))
            {
                ImGui::OpenPopup("AddComponentPopup");
            }

            if (ImGui::BeginPopup("AddComponentPopup"))
            {
                if (!m_ActiveScene->HasComponent<CameraComponent>(m_SelectedEntity))
                {
                    if (ImGui::MenuItem("Camera"))
                    {
                        auto& camera = m_ActiveScene->AddComponent<CameraComponent>(m_SelectedEntity);
                        camera.RecalculateProjection(m_ActiveScene->GetViewportSize());
                        ImGui::CloseCurrentPopup();
                    }
                }
                if (!m_ActiveScene->HasComponent<TransformComponent>(m_SelectedEntity))
                {
                    if (ImGui::MenuItem("Transform"))
                    {
                        m_ActiveScene->AddComponent<TransformComponent>(m_SelectedEntity);
                        ImGui::CloseCurrentPopup();
                    }
                }
                if (!m_ActiveScene->HasComponent<MeshComponent>(m_SelectedEntity))
                {
                    if (ImGui::MenuItem("Mesh"))
                    {
                        m_ActiveScene->AddComponent<MeshComponent>(m_SelectedEntity);
                        ImGui::CloseCurrentPopup();
                    }
                }
                if (!m_ActiveScene->HasComponent<RigidbodyComponent>(m_SelectedEntity))
                {
                    if (ImGui::MenuItem("Rigidbody"))
                    {
                        m_ActiveScene->AddComponent<RigidbodyComponent>(m_SelectedEntity);
                        ImGui::CloseCurrentPopup();
                    }
                }
                if (!m_ActiveScene->HasComponent<BoxColliderComponent>(m_SelectedEntity))
                {
                    if (ImGui::MenuItem("Box Collider"))
                    {
                        m_ActiveScene->AddComponent<BoxColliderComponent>(m_SelectedEntity);
                        ImGui::CloseCurrentPopup();
                    }
                }
                if (!m_ActiveScene->HasComponent<CapsuleColliderComponent>(m_SelectedEntity))
                {
                    if (ImGui::MenuItem("Capsule Collider"))
                    {
                        m_ActiveScene->AddComponent<CapsuleColliderComponent>(m_SelectedEntity);
                        ImGui::CloseCurrentPopup();
                    }
                }
                if (!m_ActiveScene->HasComponent<SphereColliderComponent>(m_SelectedEntity))
                {
                    if (ImGui::MenuItem("Sphere Collider"))
                    {
                        m_ActiveScene->AddComponent<SphereColliderComponent>(m_SelectedEntity);
                        ImGui::CloseCurrentPopup();
                    }
                }
                if (!m_ActiveScene->HasComponent<PlaneColliderComponent>(m_SelectedEntity))
                {
                    if (ImGui::MenuItem("Plane Collider"))
                    {
                        m_ActiveScene->AddComponent<PlaneColliderComponent>(m_SelectedEntity);
                        ImGui::CloseCurrentPopup();
                    }
                }

                ImGui::EndPopup();
            }
        }

        ImGui::End();
    }

    void App::OnMouseScroll(float xoffset, float yoffset)
    {
        if (ImGuizmo::IsUsing())
            return;

        if (m_Vp.isHovered)
        {
            m_Camera.HandleZoom(yoffset);
        }
    }

    void App::OnMouseMotion(const glm::vec2& position, const glm::vec2& delta)
    {
        if (ImGuizmo::IsUsing())
            return;

        if (m_Vp.isHovered)
        {
            m_Camera.HandleOrbit(delta);
            m_Camera.HandlePan(delta);
        }
    }

    void App::OnKeyPressed(SDL_Keycode key, SDL_Scancode scancode, SDL_EventType type, SDL_Keymod mod)
    {
        if (type != SDL_EVENT_KEY_DOWN)
        {
            return;
        }

        const bool ctrl = (mod & (SDL_KMOD_LCTRL | SDL_KMOD_RCTRL)) != 0;
        const bool shift = (mod & (SDL_KMOD_LSHIFT | SDL_KMOD_RSHIFT)) != 0;
        const bool alt = (mod & (SDL_KMOD_LALT | SDL_KMOD_RALT)) != 0;

        if (ctrl)
        {
            if (shift)
            {
                if (key == SDLK_S)
                {
                    // Save runtime on alt pressed
                    m_SaveRuntime = alt;
                    SaveSceneAs();
                }
                return;
            }

            switch (key)
            {
                case SDLK_S:
                {
                    // Save runtime on alt pressed
                    m_SaveRuntime = alt;
                    SaveScene();

                    break;
                }
                case SDLK_D:
                {
                    if (m_SelectedEntity != entt::null)
                    {
                        m_ActiveScene->DuplicateEntity(m_SelectedEntity);
                    }
                    break;
                }
                case SDLK_O:
                {
                    OpenScene();
                    break;
                }
                case SDLK_N:
                {
                    NewScene();
                    break;
                }
            }
            return;
        }

        if (shift)
        {
            if (key == SDLK_W)
            {
                m_GizmoMode = m_GizmoMode == ImGuizmo::MODE::WORLD ? ImGuizmo::MODE::LOCAL : ImGuizmo::MODE::WORLD;
            }
            return;
        }

        switch (key)
        {
            case SDLK_T:
            {
                m_GizmoOperation = ImGuizmo::OPERATION::TRANSLATE;
                break;
            }
            case SDLK_S:
            {
                m_GizmoOperation = ImGuizmo::OPERATION::SCALE;
                break;
            }
            case SDLK_R:
            {
                m_GizmoOperation = ImGuizmo::OPERATION::ROTATE;
                break;
            }
            case SDLK_DELETE:
            {
                m_ActiveScene->DestroyEntity(m_SelectedEntity);
                m_SelectedEntity = entt::null;
                break;
            }
        }
    }

    void App::SaveScene()
    {
        if (!m_EditorScene && !m_ActiveScene)
        {
            SDL_Log("SaveScene: no scene is available to save");
            return;
        }

        if (m_CurrentScenePath.empty())
        {
            SaveSceneAs();
            return;
        }

        SaveSceneToPath(m_CurrentScenePath);
    }

    void App::SaveSceneAs()
    {
        if (!m_EditorScene && !m_ActiveScene)
        {
            SDL_Log("SaveSceneAs: no scene is available to save");
            return;
        }

        m_SaveDialogDefaultLocation.clear();
        if (!m_CurrentScenePath.empty())
        {
            m_SaveDialogDefaultLocation = m_CurrentScenePath.string();
        }

        const char* defaultLocation = m_SaveDialogDefaultLocation.empty() ? nullptr : m_SaveDialogDefaultLocation.c_str();
        SDL_ShowSaveFileDialog(OnSceneSaveFileSelected, this, m_Window->GetHandle(), kSceneFileFilters, static_cast<int>(sizeof(kSceneFileFilters) / sizeof(kSceneFileFilters[0])), defaultLocation);
    }

    void App::OpenScene()
    {
        if (m_ActiveScene && m_ActiveScene->IsPlaying())
        {
            OnSceneStop();
        }

        if (m_SaveDialogDefaultLocation.empty() && !m_CurrentScenePath.empty())
        {
            m_SaveDialogDefaultLocation = m_CurrentScenePath.string();
        }

        const char* defaultLocation = m_SaveDialogDefaultLocation.empty() ? nullptr : m_SaveDialogDefaultLocation.c_str();
        SDL_ShowOpenFileDialog(
            OnSceneOpenFileSelected,
            this,
            m_Window->GetHandle(),
            kSceneFileFilters,
            static_cast<int>(sizeof(kSceneFileFilters) / sizeof(kSceneFileFilters[0])),
            defaultLocation,
            false);
    }

    void App::NewScene()
    {
        m_SelectedEntity = entt::null;

        m_ActiveScene = CreateRef<Scene>();
        m_EditorScene = m_ActiveScene->Clone();

        const glm::vec2 viewport = GetSceneViewportSize();
        if (m_ActiveScene)
        {
            m_ActiveScene->ResizeViewport(viewport);
        }
    }
    
    void App::SaveSceneToPath(const std::filesystem::path& filepath)
    {
        if (filepath.empty())
        {
            SDL_Log("SaveSceneToPath: filepath is empty");
            return;
        }
        
        Ref<Scene> sceneToSave = m_EditorScene ? m_EditorScene : m_ActiveScene;
        if (m_SaveRuntime)
            sceneToSave = m_ActiveScene;

        if (!sceneToSave)
        {
            SDL_Log("SaveSceneToPath: no scene is available to save");
            return;
        }

        std::filesystem::path destination = filepath;
        if (destination.extension().empty())
        {
            destination.replace_extension(".json");
        }

        SceneSerializer serializer(sceneToSave);
        if (!serializer.Serialize(destination))
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to save scene to %s", destination.string().c_str());
            return;
        }

        m_CurrentScenePath = destination;
        SDL_Log("Scene saved to %s", destination.string().c_str());
    }

    void App::OpenSceneFromPath(const std::filesystem::path &filepath)
    {
        if (filepath.empty())
        {
            SDL_Log("OpenSceneFromPath: filepath is empty");
            return;
        }

        std::filesystem::path scenePath = filepath;
        if (scenePath.extension().empty())
        {
            scenePath.replace_extension(".json");
        }

        if (!std::filesystem::exists(scenePath))
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Scene file does not exist: %s", scenePath.string().c_str());
            return;
        }

        if (m_ActiveScene && m_ActiveScene->IsPlaying())
        {
            OnSceneStop();
        }

        Ref<Scene> loadedScene = CreateRef<Scene>();
        SceneSerializer serializer(loadedScene);
        if (!serializer.Deserialize(scenePath))
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open scene %s", scenePath.string().c_str());
            return;
        }

        m_EditorScene = loadedScene;
        m_ActiveScene = m_EditorScene;
        const glm::vec2 viewport = GetSceneViewportSize();
        if (m_EditorScene)
        {
            m_EditorScene->ResizeViewport(viewport);
        }
        if (m_ActiveScene && m_ActiveScene != m_EditorScene)
        {
            m_ActiveScene->ResizeViewport(viewport);
        }
        m_SelectedEntity = entt::null;
        m_CurrentScenePath = scenePath;
        m_SaveDialogDefaultLocation = scenePath.string();

        SDL_Log("Scene opened from %s", scenePath.string().c_str());
    }

    void App::ProcessPendingSceneActions()
    {
        std::optional<std::filesystem::path> sceneToOpen;
        {
            std::lock_guard<std::mutex> lock(m_SceneDialogMutex);
            if (m_PendingSceneOpenPath.has_value())
            {
                sceneToOpen = std::move(m_PendingSceneOpenPath);
                m_PendingSceneOpenPath.reset();
            }
        }

        if (sceneToOpen)
        {
            OpenSceneFromPath(*sceneToOpen);
        }
    }

    void App::ProcessPendingMeshImports()
    {
        std::vector<std::filesystem::path> meshRequests;
        {
            std::lock_guard<std::mutex> lock(m_MeshImportMutex);
            if (m_PendingMeshImportQueue.empty())
            {
                return;
            }
            meshRequests.swap(m_PendingMeshImportQueue);
        }

        for (const std::filesystem::path& meshPath : meshRequests)
        {
            if (!m_ActiveScene)
            {
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Cannot import mesh %s because no active scene is loaded", meshPath.string().c_str());
                continue;
            }

            entt::entity rootEntity = entt::null;
            glm::mat4 rootTransform = glm::mat4(1.0f);
            if (m_SelectedEntity != entt::null && m_ActiveScene->IsValid(m_SelectedEntity))
            {
                rootEntity = m_SelectedEntity;
                if (m_ActiveScene->HasComponent<TransformComponent>(rootEntity))
                {
                    const TransformComponent& transform = m_ActiveScene->GetComponent<TransformComponent>(rootEntity);
                    rootTransform = math::ComposeTransform(transform);
                }
            }

            m_PendingMeshFilepath = meshPath.string();
            const auto createdEntities = m_ActiveScene->LoadModel(m_PendingMeshFilepath, rootTransform, rootEntity);
            if (!createdEntities.empty())
            {
                m_SelectedEntity = createdEntities.front();
            }
            SDL_Log("Mesh import processed: %s", meshPath.string().c_str());
        }
    }

    glm::vec2 App::GetSceneViewportSize() const
    {
        if (m_Vp.viewport.width > 0 && m_Vp.viewport.height > 0)
        {
            return { static_cast<float>(m_Vp.viewport.width), static_cast<float>(m_Vp.viewport.height) };
        }

        if (m_Window)
        {
            return { static_cast<float>(m_Window->GetWidth()), static_cast<float>(m_Window->GetHeight()) };
        }

        return { 1.0f, 1.0f };
    }

    bool App::ApplyRuntimeCamera()
    {
        if (!m_ActiveScene || !m_ActiveScene->IsPlaying())
        {
            return false;
        }

        entt::entity primaryCamera = m_ActiveScene->GetPrimaryCamera();
        if (primaryCamera == entt::null)
        {
            return false;
        }

        if (!m_ActiveScene->HasComponent<CameraComponent>(primaryCamera) || !m_ActiveScene->HasComponent<TransformComponent>(primaryCamera))
        {
            return false;
        }

        auto& cameraComponent = m_ActiveScene->GetComponent<CameraComponent>(primaryCamera);
        auto& transform = m_ActiveScene->GetComponent<TransformComponent>(primaryCamera);

        cameraComponent.RecalculateProjection(m_ActiveScene->GetViewportSize());

        m_Camera.projectionType = cameraComponent.projectionType;
        m_Camera.fov = cameraComponent.fov;
        m_Camera.nearPlane = cameraComponent.nearPlane;
        m_Camera.farPlane = cameraComponent.farPlane;
        m_Camera.orthoSize = cameraComponent.orthoSize;
        m_Camera.view = cameraComponent.view;
        m_Camera.projection = cameraComponent.projection;
        m_Camera.lens = cameraComponent.lens;
        m_Camera.postProcessing = cameraComponent.postProcessing;

        const glm::quat rotation = glm::quat(glm::radians(transform.rotation));
        const glm::vec3 forward = glm::normalize(rotation * glm::vec3(0.0f, 0.0f, -1.0f));
        const glm::vec3 up = glm::normalize(rotation * glm::vec3(0.0f, 1.0f, 0.0f));
        m_Camera.position = transform.position;
        m_Camera.target = transform.position + forward;
        m_Camera.up = up;
        m_Camera.distance = 1.0f;

        return true;
    }

    void App::OnSceneSaveFileSelected(void* userData, const char* const* filelist, int filter)
    {
        App* app = static_cast<App*>(userData);
        if (!app)
        {
            return;
        }

        app->m_SaveDialogDefaultLocation.clear();

        if (filelist == nullptr)
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Scene save dialog failed");
            return;
        }

        if (filelist[0] == nullptr)
        {
            SDL_Log("Scene save dialog cancelled");
            return;
        }

        app->SaveSceneToPath(std::filesystem::path(filelist[0]));
    }

    void App::OnSceneOpenFileSelected(void * userData, const char * const * filelist, int filter)
    {
        App* app = static_cast<App*>(userData);
        if (!app)
        {
            return;
        }

        app->m_SaveDialogDefaultLocation.clear();

        if (filelist == nullptr)
        {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Scene open dialog failed");
            return;
        }

        if (filelist[0] == nullptr)
        {
            SDL_Log("Scene open dialog cancelled");
            return;
        }

        {
            std::lock_guard<std::mutex> lock(app->m_SceneDialogMutex);
            app->m_PendingSceneOpenPath = std::filesystem::path(filelist[0]);
        }
    }

    void App::OnMeshFileSelected(void* userData, const char* const* filelist, int filter)
	{
        if (filelist[0] == nullptr)
        {
            SDL_Log("File dialog cancelled (no file selected)");
            return;
        }

        App* app = static_cast<App*>(userData);
        if (!app)
        {
            return;
        }

        {
            std::lock_guard<std::mutex> lock(app->m_MeshImportMutex);
            app->m_PendingMeshImportQueue.emplace_back(filelist[0]);
        }

        SDL_Log("Mesh import queued: %s", filelist[0]);
	}

}