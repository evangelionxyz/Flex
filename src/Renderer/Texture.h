#pragma once

#include <cstdint>
#include <string>

enum class TextureFormat
{
    RGBA8,
    RGBAF32,
};

struct TextureCreateInfo
{
    int width = 1;
    int height = 1;
    uint32_t hexColor = 0xFFFFFFFF;
    TextureFormat format = TextureFormat::RGBA8;
};

class Texture2D
{
public:
    Texture2D(const TextureCreateInfo &createInfo);
    Texture2D(const TextureCreateInfo &createInfo, void *data, size_t size);
    Texture2D(const TextureCreateInfo &createInfo, const std::string &filename);

    ~Texture2D();
    
    void Bind(int index);
    void Unbind();

    void SetData(void *data, size_t size);
    void Resize(int width, int height, unsigned int filterType);
    
    uint32_t GetWidth() const { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }
    uint32_t GetChannels() const { return m_Channels; }
    uint32_t GetHandle() const { return m_Handle; }
    int GetBindIndex() const { return m_BindIndex; }

private:
    void CreateTexture(uint32_t filterType, uint32_t clampMode);

    uint32_t m_Handle = 0;
    uint32_t m_Width = 1;
    uint32_t m_Height = 1;
    uint32_t m_Channels = 4;
    TextureFormat m_Format;
    
    int m_BindIndex = 0;
};