#include "Bloom.h"
#include <glad/glad.h>
#include <glm/glm.hpp>

static GLuint g_FullscreenVAO = 0;

void Bloom::Init(int width, int height)
{
    // Create a dummy VAO for glDrawArrays with gl_VertexID FS triangle
    if (g_FullscreenVAO == 0)
    {
        glGenVertexArrays(1, &g_FullscreenVAO);
    }

    // Load shaders (fullscreen triangle vertex)
    m_DownsampleShader.AddFromFile("resources/shaders/bloom_fullscreen.vert.glsl", GL_VERTEX_SHADER);
    m_DownsampleShader.AddFromFile("resources/shaders/bloom_downsample.frag.glsl", GL_FRAGMENT_SHADER);
    m_DownsampleShader.Compile();

    m_BlurShader.AddFromFile("resources/shaders/bloom_fullscreen.vert.glsl", GL_VERTEX_SHADER);
    m_BlurShader.AddFromFile("resources/shaders/bloom_blur.frag.glsl", GL_FRAGMENT_SHADER);
    m_BlurShader.Compile();

    CreateMipFramebuffers(width, height);
}

void Bloom::Resize(int width, int height)
{
    CreateMipFramebuffers(width, height);
}

void Bloom::CreateMipFramebuffers(int width, int height)
{
    m_Levels.clear();
    int w = width / 2; // start from half-res to save cost
    int h = height / 2;
    for (int i = 0; i < 5; ++i)
    {
        if (w <= 0 || h <= 0) break;
    FramebufferCreateInfo ciA;
    ciA.width = w; 
    ciA.height = h;
    ciA.attachments = { {Format::RGBA16F, FilterMode::LINEAR, WrapMode::CLAMP_TO_EDGE} };
    FramebufferCreateInfo ciB = ciA; // identical

    Level lvl; 
    lvl.width = w; 
    lvl.height = h;
    lvl.fbA = Framebuffer::Create(ciA);
    lvl.fbB = Framebuffer::Create(ciB);

    m_Levels.push_back(lvl);
        
        w /= 2; h /= 2;
    }
}

void Bloom::Build(uint32_t sourceTex, float threshold, float knee, int iterations)
{
    if (m_Levels.empty()) return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    uint32_t prevTex = sourceTex;
    // Downsample
    const auto maxLevels = static_cast<size_t>(glm::clamp(iterations, 1, (int)m_Levels.size()));
    glBindVertexArray(g_FullscreenVAO);
    for (size_t i = 0; i < maxLevels; ++i)
    {
        auto &lvl = m_Levels[i];
        Viewport vp{0,0,(uint32_t)lvl.width,(uint32_t)lvl.height};
        lvl.fbA->Bind(vp);
        glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);

        m_DownsampleShader.Use();
        glBindTextureUnit(0, prevTex);
        m_DownsampleShader.SetUniform("u_Src", 0);
        m_DownsampleShader.SetUniform("u_Threshold", threshold);
        m_DownsampleShader.SetUniform("u_Knee", knee);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        prevTex = lvl.fbA->GetColorAttachment(0);
    }

    // Ping-pong blur (horizontal/vertical approximation using direction uniform if implemented, else two passes)
    for (size_t i = 0; i < maxLevels; ++i)
    {
        auto &lvl = m_Levels[i];
        // Pass 1: A -> B
        Viewport vp{0,0,(uint32_t)lvl.width,(uint32_t)lvl.height};
        lvl.fbB->Bind(vp);
        glClear(GL_COLOR_BUFFER_BIT);
        m_BlurShader.Use();
        glBindTextureUnit(0, lvl.fbA->GetColorAttachment(0));
        m_BlurShader.SetUniform("u_Src", 0);
        m_BlurShader.SetUniform("u_Horizontal", 1); // if shader uses it
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Pass 2: B -> A
        lvl.fbA->Bind(vp);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindTextureUnit(0, lvl.fbB->GetColorAttachment(0));
        m_BlurShader.SetUniform("u_Src", 0);
        m_BlurShader.SetUniform("u_Horizontal", 0);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
}

void Bloom::BindTextures() const
{
    for (size_t i = 0; i < m_Levels.size() && i < 5; ++i)
        glBindTextureUnit(2 + (uint32_t)i, m_Levels[i].fbA->GetColorAttachment(0));
}
