// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef APP_H
#define APP_H

#include "Types.h"

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
#include "Scene/Scene.h"
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

#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>

namespace flex
{
    namespace
    {
        constexpr int RENDER_MODE_COLOR = 0;
        constexpr int RENDER_MODE_NORMALS = 1;
        constexpr int RENDER_MODE_METALLIC = 2;
        constexpr int RENDER_MODE_ROUGHNESS = 3;
        constexpr int RENDER_MODE_DEPTH = 4;
    }

    class Screen
    {
    public:
        Screen()
        {
            Create();
        }

        void Render(uint32_t texture, uint32_t depthTex, const flex::Camera& camera, const flex::PostProcessing& postProcessing)
        {
			shader->Use();
            glBindTextureUnit(0, texture);
            shader->SetUniform("u_ColorTexture", 0);
            glBindTextureUnit(1, depthTex);
            shader->SetUniform("u_DepthTexture", 1);

            shader->SetUniform("u_FocalLength", camera.lens.focalLength);
            shader->SetUniform("u_FocalDistance", camera.lens.focalDistance);
            shader->SetUniform("u_FStop", camera.lens.fStop);
            shader->SetUniform("u_FocusRange", camera.lens.focusRange);
            shader->SetUniform("u_BlurAmount", camera.lens.blurAmount);
            shader->SetUniform("u_InverseProjection", inverseProjection);
            shader->SetUniform("u_Exposure", camera.lens.exposure);
            shader->SetUniform("u_Gamma", camera.lens.gamma);
            shader->SetUniform("u_EnableDOF", camera.lens.enableDOF ? 1 : 0);
            shader->SetUniform("u_EnableVignette", postProcessing.enableVignette ? 1 : 0);
            shader->SetUniform("u_EnableChromAb", postProcessing.enableChromAb ? 1 : 0);
            shader->SetUniform("u_EnableBloom", postProcessing.enableBloom ? 1 : 0);
            shader->SetUniform("u_EnableSSAO", postProcessing.enableSSAO ? 1 : 0);
            shader->SetUniform("u_AOIntensity", postProcessing.aoIntensity);
            shader->SetUniform("u_DebugSSAO", postProcessing.debugSSAO ? 1 : 0);
            shader->SetUniform("u_VignetteRadius", postProcessing.vignetteRadius);
            shader->SetUniform("u_VignetteSoftness", postProcessing.vignetteSoftness);
            shader->SetUniform("u_VignetteIntensity", postProcessing.vignetteIntensity);
            shader->SetUniform("u_VignetteColor", postProcessing.vignetteColor);
            shader->SetUniform("u_ChromaticAberrationAmount", postProcessing.chromAbAmount);
            shader->SetUniform("u_ChromaticAberrationRadial", postProcessing.chromAbRadial);

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
            vertexBuffer->SetAttributes({ {VertexAttribType::VECTOR_FLOAT_2} }, sizeof(glm::vec2));
            indexBuffer = std::make_shared<IndexBuffer>(indices.data(), static_cast<uint32_t>(indices.size()));

            vertexArray->SetVertexBuffer(vertexBuffer);
            vertexArray->SetIndexBuffer(indexBuffer);

            assert(glGetError() == GL_NO_ERROR);

            shader = Renderer::CreateShaderFromFile(
                {
                    ShaderData{ "Resources/shaders/screen.vert.glsl", GL_VERTEX_SHADER },
                    ShaderData{ "Resources/shaders/screen.frag.glsl", GL_FRAGMENT_SHADER },
				}, "ScreenShader");
        }

        std::shared_ptr<VertexArray> vertexArray;
        std::shared_ptr<VertexBuffer> vertexBuffer;
        std::shared_ptr<IndexBuffer> indexBuffer;

        Ref<Shader> shader;
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

    struct ViewportData
    {
        Viewport viewport;
        bool isHovered = false;
    };

    struct FrameData
    {
        float fps = 0.0f;
        float deltaTime = 0.0f;

    };

    class App
    {
    public:
        App(int argc,  char **argv);
        ~App();
        
        void Run();
    private:
        void OnScenePlay();
        void OnSceneStop();

        void OnImGuiRender();
        void UIViewport();
        void UISettings();

        void UISceneHierarchy();
        void UISceneProperties();

        void OnMouseScroll(float xoffset, float yoffset);
        void OnMouseMotion(const glm::vec2 &position, const glm::vec2 &delta);

        static void OnMeshFileSelected(void* userData, const char* const* filelist, int filter);

    private:
        Ref<Window> m_Window;
        Ref<Framebuffer> m_SceneFB;
        Ref<Framebuffer> m_ViewportFB;
        Ref<Texture2D> m_EnvMap;

        Ref<CascadedShadowMap> m_CSM;
        Ref<Bloom> m_Bloom;
        Ref<SSAO> m_SSAO;
        Ref<Screen> m_Screen;

        Ref<Scene> m_ActiveScene;
        Ref<Scene> m_EditorScene;

        entt::entity m_SelectedEntity = { entt::null };

        ViewportData m_Vp;
        Camera m_Camera;
        SceneData m_SceneData;
        FrameData m_FrameData;

        std::string m_PendingMeshFilepath;

        ImGuizmo::OPERATION m_GizmoOperation = ImGuizmo::TRANSLATE;
        ImGuizmo::MODE m_GizmoMode = ImGuizmo::LOCAL;
    };
}

#endif