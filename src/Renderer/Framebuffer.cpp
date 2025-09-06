#include "Framebuffer.h"

#include <glad/glad.h>

#define MAX_RESOLUTION 8192

Framebuffer::Framebuffer(const FramebufferCreateInfo &createInfo)
    : m_CreateInfo(createInfo)
{
    m_Viewport.width = createInfo.width;
    m_Viewport.height = createInfo.height;
    
    glCreateFramebuffers(1, &m_Handle);
    glBindFramebuffer(GL_FRAMEBUFFER, m_Handle);

    CreateAttachments();
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE && "Failed to create framebuffer");
}

Framebuffer::~Framebuffer()
{
    glDeleteFramebuffers(1, &m_Handle);
    
    // Delete old color attachments
    for (uint32_t texture : m_ColorAttachments)
    {
        glDeleteTextures(1, &texture);
    }

    m_ColorAttachments.clear();
    
    if (m_DepthAttachment != 0)
    {
        glDeleteTextures(1, &m_DepthAttachment);
    }
}

int Framebuffer::ReadPixel(int index, int x, int y)
{
    if (index < 0 || index >= static_cast<int>(m_ColorAttachments.size()))
        return -1;

    int pixelData;
    glReadBuffer(GL_COLOR_ATTACHMENT0 + index);
	glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_INT, &pixelData);
    return pixelData;
}

void Framebuffer::Resize(uint32_t width, uint32_t height)
{
    const bool negativeResolution = width <= 0 || height < 0;
    const bool sameResolution = width == m_Viewport.width && height == m_Viewport.height;
    const bool outOfResolutionRange = width > MAX_RESOLUTION || height > MAX_RESOLUTION;
    if (negativeResolution || outOfResolutionRange || sameResolution)
        return;

    glDeleteFramebuffers(1, &m_Handle);
    
    glCreateFramebuffers(1, &m_Handle);
    glBindFramebuffer(GL_FRAMEBUFFER, m_Handle);

    m_CreateInfo.width = width;
    m_CreateInfo.height = height;

    // Delete old color attachments
    for (uint32_t texture : m_ColorAttachments)
    {
        glDeleteTextures(1, &texture);
    }

    m_ColorAttachments.clear();

    // Delete old depth attachment
    if (m_DepthAttachment != 0)
    {
        glDeleteTextures(1, &m_DepthAttachment);
        m_DepthAttachment = 0;
    }

    CreateAttachments();

    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE && "Failed to resize framebuffer");
}

void Framebuffer::Bind(const Viewport &viewport)
{
    m_Viewport = viewport;
    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
    // Ensure we render into our framebuffer, not the default one
    glBindFramebuffer(GL_FRAMEBUFFER, m_Handle);
}

void Framebuffer::ClearColorAttachment(int index, const glm::vec4 &color)
{
    if (index < 0 || index >= static_cast<int>(m_ColorAttachments.size()))
        return;

    float c[4] = { color.r, color.g, color.b, color.a };
    glClearBufferfv(GL_COLOR, index, c);
}

void Framebuffer::CheckSize(uint32_t width, uint32_t height)
{
    if (width != m_Viewport.width || height != m_Viewport.height)
        Resize(width, height);
}

uint32_t Framebuffer::GetColorAttachment(int index)
{
    if (index < 0 || index >= static_cast<int>(m_ColorAttachments.size()))
        return 0;
    
    return m_ColorAttachments[index];
}

std::shared_ptr<Framebuffer> Framebuffer::Create(const FramebufferCreateInfo &createInfo)
{
    return std::make_shared<Framebuffer>(createInfo);
}

void Framebuffer::CreateAttachments()
{
    // Create depth and color attachments
    for (const auto &attachment : m_CreateInfo.attachments)
    {
        TextureCreateInfo texCreateInfo;
        texCreateInfo.format = attachment.format;
        texCreateInfo.width = m_CreateInfo.width;
        texCreateInfo.height = m_CreateInfo.height;

        texCreateInfo.filter = attachment.filter;
        texCreateInfo.clampMode = attachment.wrap;

        GLenum internalFormat = ToGLInternalFormat(attachment.format);
        GLenum format = ToGLFormat(attachment.format);

        if (attachment.format == Format::DEPTH24STENCIL8)
        {
            glCreateTextures(GL_TEXTURE_2D, 1, &m_DepthAttachment);
            glBindTexture(GL_TEXTURE_2D, m_DepthAttachment);
            
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

            glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, m_CreateInfo.width, m_CreateInfo.height);

            // glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, m_CreateInfo.width, m_CreateInfo.height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_DepthAttachment, 0);
        }
        else
        {
            uint32_t &texture = m_ColorAttachments.emplace_back();
            
            glCreateTextures(GL_TEXTURE_2D, 1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, texCreateInfo.width, texCreateInfo.height, 0, format, GL_UNSIGNED_BYTE, nullptr);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + m_ColorAttachments.size() - 1, GL_TEXTURE_2D, texture, 0);
        }
    }

    GLenum buffers[4] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3 };
	glDrawBuffers(m_ColorAttachments.size(), buffers);

    // Set up draw buffers for multiple color attachments
    if (!m_ColorAttachments.empty())
    {
        std::vector<GLenum> drawBuffers;
        for (size_t i = 0; i < m_ColorAttachments.size(); ++i)
        {
            drawBuffers.push_back(GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i));
        }
        glDrawBuffers(static_cast<GLsizei>(drawBuffers.size()), drawBuffers.data());
    }
}
