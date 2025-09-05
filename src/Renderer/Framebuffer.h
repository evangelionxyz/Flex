#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>

#include "Texture.h"

struct FramebufferAttachment
{
    Format format;
    FilterMode filter = FilterMode::LINEAR;
    ClampMode clamp = ClampMode::CLAMP_TO_EDGE;
};

struct Viewport
{
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 1;
    uint32_t height = 1;
};

struct FramebufferCreateInfo
{
    uint32_t width = 1280;
    uint32_t height = 720;

    std::vector<FramebufferAttachment> attachments;
};

class Framebuffer
{
public:
    Framebuffer(const FramebufferCreateInfo &createInfo);
    ~Framebuffer();

    int ReadPixel(int index, int x, int y);
    void Resize(uint32_t width, uint32_t height);
    void Bind(const Viewport &viewport);

    void ClearColorAttachment(int index, const glm::vec4 &color);

    void CheckSize(uint32_t width, uint32_t height);
    
    uint32_t GetColorAttachment(int index);

    uint32_t GetHandle() const { return m_Handle; }
    uint32_t GetWidth() const { return m_CreateInfo.width; }
    uint32_t GetHeight() const { return m_CreateInfo.height; }

    static std::shared_ptr<Framebuffer> Create(const FramebufferCreateInfo &createInfo);

private:
    void CreateAttachments();
    
    uint32_t m_Handle;
    uint32_t m_DepthAttachment = 0;
    Viewport m_Viewport;
    FramebufferCreateInfo m_CreateInfo;
    std::vector<uint32_t> m_ColorAttachments;

};