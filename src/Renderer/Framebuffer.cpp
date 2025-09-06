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
    for (const auto &attachment : m_CreateInfo.attachments)
    {
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
            float borderColor[] = {1.0f,1.0f,1.0f,1.0f};
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
            glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, m_CreateInfo.width, m_CreateInfo.height);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_DepthAttachment, 0);
        }
        else
        {
            uint32_t &tex = m_ColorAttachments.emplace_back();
            glCreateTextures(GL_TEXTURE_2D, 1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            bool isFloat = internalFormat == GL_RGB16F || internalFormat == GL_RGB32F || internalFormat == GL_RGBA16F || internalFormat == GL_RGBA32F;
            GLenum dataType = isFloat ? GL_FLOAT : GL_UNSIGNED_BYTE; // could refine to GL_HALF_FLOAT for 16F
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_CreateInfo.width, m_CreateInfo.height, 0, format, dataType, nullptr);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + (GLuint)(m_ColorAttachments.size()-1), GL_TEXTURE_2D, tex, 0);
#if defined(GL_VERSION_4_4)
            if (isFloat)
            {
                float zero[4] = {0,0,0,0};
                glClearTexImage(tex, 0, format, dataType, zero);
            }
            else
            {
                unsigned int zero[4] = {0,0,0,0};
                glClearTexImage(tex, 0, format, GL_UNSIGNED_INT, zero);
            }
#endif
        }
    }

    if (!m_ColorAttachments.empty())
    {
        std::vector<GLenum> bufs; bufs.reserve(m_ColorAttachments.size());
        for (size_t i=0;i<m_ColorAttachments.size();++i) bufs.push_back(GL_COLOR_ATTACHMENT0 + (GLenum)i);
        glDrawBuffers((GLsizei)bufs.size(), bufs.data());
    }
}
