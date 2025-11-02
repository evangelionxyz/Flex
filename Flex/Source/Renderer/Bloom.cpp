// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "Bloom.h"
#include <glad/glad.h>
#include <glm/glm.hpp>

namespace flex
{
    Bloom::Bloom(int width, int height) : m_Width(width), m_Height(height)
    {
        // Create a dummy VAO for glDrawArrays with gl_VertexID FS triangle
        if (m_Vao == 0)
        {
            glGenVertexArrays(1, &m_Vao);
        }

        // Load high-quality shaders
        m_DownsampleShader.AddFromFile("Resources/shaders/bloom_fullscreen.vert.glsl", GL_VERTEX_SHADER);
        m_DownsampleShader.AddFromFile("Resources/shaders/bloom_downsample.frag.glsl", GL_FRAGMENT_SHADER);
        m_DownsampleShader.Compile();

        m_BlurShader.AddFromFile("Resources/shaders/bloom_fullscreen.vert.glsl", GL_VERTEX_SHADER);
        m_BlurShader.AddFromFile("Resources/shaders/bloom_blur.frag.glsl", GL_FRAGMENT_SHADER);
        m_BlurShader.Compile();
        
        m_UpsampleShader.AddFromFile("Resources/shaders/bloom_fullscreen.vert.glsl", GL_VERTEX_SHADER);
        m_UpsampleShader.AddFromFile("Resources/shaders/bloom_upsample.frag.glsl", GL_FRAGMENT_SHADER);
        m_UpsampleShader.Compile();

        CreateMipFramebuffers(width, height);
    }

    Bloom::~Bloom()
    {
        if (m_Vao != 0)
        {
            glDeleteVertexArrays(1, &m_Vao);
        }

        m_Vao = 0;
    }

    void Bloom::Resize(int width, int height)
    {
        m_Width = width;
        m_Height = height;
        CreateMipFramebuffers(width, height);
    }

    void Bloom::CreateMipFramebuffers(int width, int height)
    {
        m_Levels.clear();
        int w = width / 2; // Start from half-res to save performance
        int h = height / 2;
        
        // Create mip chain levels
        for (int i = 0; i < 8; ++i) // More levels for higher quality
        {
            if (w <= 4 || h <= 4) // Stop when too small
                break;

            FramebufferCreateInfo createInfo;
            createInfo.width = w; 
            createInfo.height = h;
            createInfo.attachments = { {Format::RGBA16F, FilterMode::LINEAR, WrapMode::CLAMP_TO_EDGE} };

            Level lvl; 
            lvl.width = w;
            lvl.height = h;

            if (!lvl.fbDown)
            {
                lvl.fbDown = Framebuffer::Create(createInfo);
            }
            else
            {
                lvl.fbDown->Resize(w, h);
            }

            if (!lvl.fbBlurH)
            {
                lvl.fbBlurH = Framebuffer::Create(createInfo);
            }
            else
            {
                lvl.fbBlurH->Resize(w, h);
            }

            if (!lvl.fbBlurV)
            {
                lvl.fbBlurV = Framebuffer::Create(createInfo);
            }
            else
            {
                lvl.fbBlurV->Resize(w, h);
            }

            m_Levels.push_back(lvl);
            
            w /= 2;
            h /= 2;
        }
        
        // Create final framebuffer for upsampling result
        if (!m_Levels.empty())
        {
            FramebufferCreateInfo finalInfo;
            finalInfo.width = width / 2;
            finalInfo.height = height / 2;
            finalInfo.attachments = { {Format::RGBA16F, FilterMode::LINEAR, WrapMode::CLAMP_TO_EDGE} };
            if (!m_FinalFB)
            {
                m_FinalFB = Framebuffer::Create(finalInfo);
            }
            else
            {
                m_FinalFB->Resize(finalInfo.width, finalInfo.height);
            }
        }
    }

    void Bloom::Build(uint32_t sourceTex)
    {
        if (m_Levels.empty())
            return;

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glBindVertexArray(m_Vao);

        uint32_t prevTex = sourceTex;

        // Phase 1: Downsample with high-quality 13-tap filter
        const auto maxLevels = static_cast<size_t>(glm::clamp(settings.iterations, 1, (int)m_Levels.size()));
        
        for (size_t i = 0; i < maxLevels; ++i)
        {
            auto &lvl = m_Levels[i];
            Viewport vp{0, 0, (uint32_t)lvl.width, (uint32_t)lvl.height};
            lvl.fbDown->Bind(vp);
            
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);

            m_DownsampleShader.Use();
            glBindTextureUnit(0, prevTex);
            m_DownsampleShader.SetUniform("u_Src", 0);
            m_DownsampleShader.SetUniform("u_Intensity", settings.intensity);
            m_DownsampleShader.SetUniform("u_Knee", settings.knee);
            
            // Only apply threshold on first level
            if (i == 0)
            {
                m_DownsampleShader.SetUniform("u_Threshold", settings.threshold);
            }
            else
            {
                m_DownsampleShader.SetUniform("u_Threshold", 0.0f);
            }
            
            glDrawArrays(GL_TRIANGLES, 0, 3);
            prevTex = lvl.fbDown->GetColorAttachment(0);
        }

        // Phase 2: High-quality separable blur for each level
        for (size_t i = 0; i < maxLevels; ++i)
        {
            auto &lvl = m_Levels[i];
            Viewport vp{0, 0, (uint32_t)lvl.width, (uint32_t)lvl.height};
            
            // Horizontal blur: fbDown -> fbBlurH
            lvl.fbBlurH->Bind(vp);
            glClear(GL_COLOR_BUFFER_BIT);
            
            m_BlurShader.Use();
            glBindTextureUnit(0, lvl.fbDown->GetColorAttachment(0));
            m_BlurShader.SetUniform("u_Src", 0);
            m_BlurShader.SetUniform("u_Horizontal", 1);
            glDrawArrays(GL_TRIANGLES, 0, 3);

            // Vertical blur: fbBlurH -> fbBlurV
            lvl.fbBlurV->Bind(vp);
            glClear(GL_COLOR_BUFFER_BIT);
            
            glBindTextureUnit(0, lvl.fbBlurH->GetColorAttachment(0));
            m_BlurShader.SetUniform("u_Src", 0);
            m_BlurShader.SetUniform("u_Horizontal", 0);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }

        // Phase 3: Upsample and combine from smallest to largest
        if (maxLevels > 0 && m_FinalFB)
        {
            // Start with the smallest level
            uint32_t currentTex = m_Levels[maxLevels - 1].fbBlurV->GetColorAttachment(0);
            
            // Upsample and combine each level
            for (int i = (int)maxLevels - 2; i >= 0; --i)
            {
                auto &lvl = m_Levels[i];
                Viewport vp{0, 0, (uint32_t)lvl.width, (uint32_t)lvl.height};
                
                if (i == 0)
                {
                    // Final level - render to final framebuffer
                    m_FinalFB->Bind(vp);
                }
                else
                {
                    // Intermediate level - use the blur framebuffer as temp
                    lvl.fbBlurH->Bind(vp);
                }
                
                glClear(GL_COLOR_BUFFER_BIT);
                
                m_UpsampleShader.Use();
                glBindTextureUnit(0, currentTex);  // Lower resolution
                glBindTextureUnit(1, lvl.fbBlurV->GetColorAttachment(0));  // Current level
                m_UpsampleShader.SetUniform("u_LowRes", 0);
                m_UpsampleShader.SetUniform("u_HighRes", 1);
                m_UpsampleShader.SetUniform("u_Radius", settings.radius * (float)(i + 1));
                
                glDrawArrays(GL_TRIANGLES, 0, 3);
                
                currentTex = (i == 0) ? m_FinalFB->GetColorAttachment(0) : lvl.fbBlurH->GetColorAttachment(0);
            }
        }
    }

    void Bloom::BindTextures() const
    {
        for (size_t i = 0; i < m_Levels.size() && i < 5; ++i)
        {
            glBindTextureUnit(2 + (uint32_t)i, m_Levels[i].fbBlurV->GetColorAttachment(0));
        }
    }

    uint32_t Bloom::GetBloomTexture() const
    {
        if (m_FinalFB)
            return m_FinalFB->GetColorAttachment(0);
        return 0;
    }
}