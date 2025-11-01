// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef TEXTURE_H
#define TEXTURE_H

#include <cstdint>
#include <string>
#include <cassert>

#include <memory>

#include "RendererCommon.h"

namespace flex
{
    struct TextureCreateInfo
    {
        int width = 1;
        int height = 1;
        bool flip = true;
        Format format = Format::RGBA8;
        FilterMode filter = FilterMode::LINEAR;
        WrapMode clampMode = WrapMode::REPEAT;
    };

    class Texture2D
    {
    public:
        Texture2D(const TextureCreateInfo &createInfo);
        Texture2D(const TextureCreateInfo &createInfo, uint32_t hexColor);
        Texture2D(const TextureCreateInfo &createInfo, void *data, size_t size);
        Texture2D(const TextureCreateInfo &createInfo, const std::string &filename);

        ~Texture2D();
        
        void Bind(int index);
        void Unbind();

        void SetData(void *data, size_t size);
        void Resize(int width, int height, unsigned int filterType);
        
        uint32_t GetWidth() const { return m_CreateInfo.width; }
        uint32_t GetHeight() const { return m_CreateInfo.height; }

        WrapMode GetClampMode() const { return m_CreateInfo.clampMode; }
        Format GetFormat() const { return m_CreateInfo.format; }

        uint32_t GetChannels() const { return m_Channels; }
        uint32_t GetHandle() const { return m_Handle; }
        int GetBindIndex() const { return m_BindIndex; }

        static std::shared_ptr<Texture2D> Create(const TextureCreateInfo &createInfo);
        static std::shared_ptr<Texture2D> Create(const TextureCreateInfo &createInfo, uint32_t hexColor);
        static std::shared_ptr<Texture2D> Create(const TextureCreateInfo &createInfo, void *data, size_t size);
        static std::shared_ptr<Texture2D> Create(const TextureCreateInfo &createInfo, const std::string &filename);

    private:
        void CreateTexture();

        uint32_t m_Handle = 0;
        uint32_t m_Channels = 4;

        TextureCreateInfo m_CreateInfo;
        
        int m_BindIndex = 0;
    };
}

#endif