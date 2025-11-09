// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "SSAO.h"

#include "Renderer.h"

#include <glad/glad.h>
#include <random>
#include <glm/glm.hpp>

namespace flex
{
    SSAO::SSAO(int width, int height)
    {
        m_Width = width;
        m_Height = height;
        glGenVertexArrays(1, &m_Vao);

        m_AOShader = Renderer::CreateShaderFromFile(
            {
                ShaderData{ "Resources/shaders/ssao_fullscreen.vert.glsl", GL_VERTEX_SHADER },
                ShaderData{ "Resources/shaders/ssao.frag.glsl", GL_FRAGMENT_SHADER },
			}, "SSAO");

        m_BlurShader = Renderer::CreateShaderFromFile(
            {
                ShaderData{ "Resources/shaders/ssao_fullscreen.vert.glsl", GL_VERTEX_SHADER },
                ShaderData{ "Resources/shaders/ssao_blur.frag.glsl", GL_FRAGMENT_SHADER },
            }, "SSAOBlur");

        BuildKernel();
        BuildNoise();
        CreateFramebuffers(width, height);
    }

    void SSAO::Resize(int width, int height)
    {
        const bool negativeValue = width <= 0 || height <= 0;
        const bool sameValue = width == m_Width && height == m_Height;
        if (sameValue || negativeValue)
        {
            return;
        }
        
        m_Width = width;
        m_Height = height;
        m_AOFB->Resize(m_Width, m_Height);
        m_BlurFB->Resize(m_Width, m_Height);
    }

    void SSAO::CreateFramebuffers(int width, int height)
    {
        FramebufferCreateInfo aoInfo; aoInfo.width = width; aoInfo.height = height;
        aoInfo.attachments = { {Format::R8, FilterMode::LINEAR, WrapMode::CLAMP_TO_EDGE} };
        m_AOFB = Framebuffer::Create(aoInfo);
        m_BlurFB = Framebuffer::Create(aoInfo);
    }

    void SSAO::BuildKernel()
    {
        m_Kernel.clear();
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::default_random_engine rng;

        for (int i = 0; i < 32; ++i)
        {
            glm::vec3 sample(
                dist(rng) * 2.0f - 1.0f,
                dist(rng) * 2.0f - 1.0f,
                dist(rng)
            );
            sample = glm::normalize(sample);
            sample *= dist(rng);
            float scale = float(i) / 32.0f;
            // Accelerate samples near origin
            scale = glm::mix(0.1f, 1.0f, scale * scale);
            sample *= scale;
            m_Kernel.push_back(glm::vec4(sample, 0.0f));
        }
    }

    void SSAO::BuildNoise()
    {
        if (m_NoiseTex) glDeleteTextures(1, &m_NoiseTex);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        std::default_random_engine rng;
        std::vector<glm::vec3> noise; noise.reserve(16);
        for (int i=0;i<16;++i)
        {
            noise.emplace_back(
                dist(rng) * 2.0f - 1.0f,
                dist(rng) * 2.0f - 1.0f,
                0.0f
            );
        }
        glCreateTextures(GL_TEXTURE_2D, 1, &m_NoiseTex);
        glBindTexture(GL_TEXTURE_2D, m_NoiseTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, noise.data());
    }

    void SSAO::Generate(uint32_t depthTex, const glm::mat4 &proj, float radius, float bias, float power)
    {
        if (!m_AOFB || !m_BlurFB)
        {
            return;
        }
        
        // step 1 raw AO
        Viewport vp{0,0,(uint32_t)m_Width,(uint32_t)m_Height};
        m_AOFB->Bind(vp);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glBindVertexArray(m_Vao);
        glClearColor(1,1,1,1); glClear(GL_COLOR_BUFFER_BIT);
        m_AOShader->Use();
        glBindTextureUnit(0, depthTex); // sampler2D u_Depth
        glBindTextureUnit(1, m_NoiseTex);
        m_AOShader->SetUniform("u_Depth", 0);
        m_AOShader->SetUniform("u_Noise", 1);
        m_AOShader->SetUniform("u_Radius", radius);
        m_AOShader->SetUniform("u_Bias", bias);
        m_AOShader->SetUniform("u_Power", power);
        glm::mat4 invProj = glm::inverse(proj);
        m_AOShader->SetUniform("u_Projection", proj);
        m_AOShader->SetUniform("u_ProjectionInv", invProj);
        for (int i=0;i<32;++i)
        {
            m_AOShader->SetUniform(("u_Samples["+std::to_string(i)+"]").c_str(), glm::vec3(m_Kernel[i]));
        }
        glDrawArrays(GL_TRIANGLES,0,3);

        // step 2 simple separable blur (horizontal+vertical in one pass for simplicity)
        m_BlurFB->Bind(vp);
        glClear(GL_COLOR_BUFFER_BIT);
        m_BlurShader->Use();
        glBindTextureUnit(0, m_AOFB->GetColorAttachment(0));
        m_BlurShader->SetUniform("u_Src",0);
        glBindVertexArray(m_Vao);
        glDrawArrays(GL_TRIANGLES,0,3);
    }
}
