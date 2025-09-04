#include "Texture.h"

#include <stb_image.h>
#include <glad/glad.h>
#include <iostream>
#include <cassert>

Texture2D::Texture2D(const TextureCreateInfo &createInfo)
    : m_Format(createInfo.format)
{
    m_Width = static_cast<uint32_t>(createInfo.width);
    m_Height = static_cast<uint32_t>(createInfo.height);
    m_Channels = 4; // RGBA format
    
    // Extract RGBA components from hex color (assuming ARGB format: 0xAARRGGBB)
    uint8_t a = (createInfo.hexColor >> 24) & 0xFF; // Alpha
    uint8_t r = (createInfo.hexColor >> 16) & 0xFF; // Red
    uint8_t g = (createInfo.hexColor >> 8) & 0xFF;  // Green
    uint8_t b = createInfo.hexColor & 0xFF;         // Blue
    
    // Create pixel data array
    size_t totalPixels = createInfo.width * createInfo.height;
    uint8_t* pixelData = new uint8_t[totalPixels * 4]; // 4 bytes per pixel (RGBA)
    
    // Fill the array with the color
    for (size_t i = 0; i < totalPixels; ++i)
    {
        pixelData[i * 4 + 0] = r; // Red
        pixelData[i * 4 + 1] = g; // Green
        pixelData[i * 4 + 2] = b; // Blue
        pixelData[i * 4 + 3] = a; // Alpha
    }
    
    std::cout << "Creating solid color texture: " << std::hex << createInfo.hexColor << std::dec 
              << " (R:" << static_cast<int>(r) << " G:" << static_cast<int>(g) 
              << " B:" << static_cast<int>(b) << " A:" << static_cast<int>(a) << ")" 
              << " - Size: " << createInfo.width << "x" << createInfo.height << std::endl;
    
    CreateTexture(GL_LINEAR, GL_REPEAT);

    GLenum internalFormat = GL_RGBA8;
    GLenum format = GL_RGBA;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, createInfo.width, createInfo.height, 0, format, GL_UNSIGNED_BYTE, pixelData);

    delete[] pixelData;
}

Texture2D::Texture2D(const TextureCreateInfo &createInfo, void *data, size_t size)
    : m_Format(createInfo.format)
{
    m_Width = static_cast<uint32_t>(createInfo.width);
    m_Height = static_cast<uint32_t>(createInfo.height);
    m_Channels = 4; // Assuming RGBA format
    
    CreateTexture(GL_LINEAR, GL_REPEAT);

    switch (createInfo.format)
    {
        case TextureFormat::RGBA8:
        {
            GLenum internalFormat = GL_RGBA8;
            GLenum format = GL_RGBA;
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_Width, m_Height, 0, format, GL_UNSIGNED_BYTE, data);
            break;
        }

        case TextureFormat::RGBAF32:
        {
            GLenum internalFormat = GL_RGBA32F;
            GLenum format = GL_RGBA;
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_Width, m_Height, 0, format, GL_FLOAT, data);
            break;
        }
    }
}

Texture2D::Texture2D(const TextureCreateInfo &createInfo, const std::string &filename)
    : m_Format(createInfo.format)
{
    // Don't flip HDR textures - they are typically stored in the correct orientation
    bool isHDR = filename.ends_with(".hdr") || filename.ends_with(".exr");
    stbi_set_flip_vertically_on_load(isHDR ? 0 : 1);

    int width, height, loadedChannels;
    
    // Create texture with appropriate wrap mode for HDR environment maps
    uint32_t wrapMode = isHDR ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    CreateTexture(GL_LINEAR, wrapMode);

    switch (createInfo.format)
    {
        case TextureFormat::RGBA8:
        {
            uint8_t *data = stbi_load(filename.c_str(), &width, &height, &loadedChannels, 4);
            if (!data)
            {
                assert(false && "Failed to load texture");
                return;
            }

            m_Width = static_cast<uint32_t>(width);
            m_Height = static_cast<uint32_t>(height);
            m_Channels = 4; // forced RGBA

            std::cout << "Loaded texture: " << filename << " - Width: " << width << ", Height: " << height << ", Channels (src->dst): " << loadedChannels << "->" << m_Channels << std::endl;

            GLenum internalFormat = GL_RGBA8;
            GLenum format = GL_RGBA;
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_Width, m_Height, 0, format, GL_UNSIGNED_BYTE, data);

            if (data)
            {
                stbi_image_free(data);
            }
            break;
        }

        case TextureFormat::RGBAF32:
        {
            // For HDR files, load as float with 3 channels (RGB) then expand to 4 (RGBA)
            float *data = stbi_loadf(filename.c_str(), &width, &height, &loadedChannels, 0);
            if (!data)
            {
                assert(false && "Failed to load HDR texture");
                return;
            }

            m_Width = static_cast<uint32_t>(width);
            m_Height = static_cast<uint32_t>(height);
            
            std::cout << "Loaded HDR texture: " << filename << " - Width: " << width << ", Height: " << height 
                      << ", Channels: " << loadedChannels << std::endl;

            // Determine format based on loaded channels
            GLenum internalFormat, format;
            if (loadedChannels == 3)
            {
                internalFormat = GL_RGB32F;
                format = GL_RGB;
                m_Channels = 3;
            }
            else if (loadedChannels == 4)
            {
                internalFormat = GL_RGBA32F;
                format = GL_RGBA;
                m_Channels = 4;
            }
            else
            {
                // Fallback to RGB for unusual channel counts
                internalFormat = GL_RGB32F;
                format = GL_RGB;
                m_Channels = 3;
            }

            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_Width, m_Height, 0, format, GL_FLOAT, data);
            
            // Check for OpenGL errors
            GLenum error = glGetError();
            if (error != GL_NO_ERROR)
            {
                std::cout << "OpenGL error after uploading HDR texture: " << error << std::endl;
            }

            if (data)
            {
                stbi_image_free(data);
            }
            break;
        }
    }    
}

void Texture2D::SetData(void *data, size_t size)
{
    assert(((m_Width * m_Height * m_Channels) == size) && "Invalid image size");

    switch (m_Format)
    {
        case TextureFormat::RGBA8:
        {
            GLenum internalFormat = GL_RGBA8;
            GLenum format = GL_RGBA;

            glBindTexture(GL_TEXTURE_2D, m_Handle);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, format, GL_UNSIGNED_BYTE, data);
            break;
        }
        case TextureFormat::RGBAF32:
        {
            GLenum internalFormat = GL_RGBA32F;
            GLenum format = GL_RGBA;

            glBindTexture(GL_TEXTURE_2D, m_Handle);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, format, GL_FLOAT, data);
            break;
        }
    }
    
    
}

void Texture2D::Resize(int width, int height, unsigned int filterType)
{
    // If the size is already correct, do nothing
    if (static_cast<uint32_t>(width) == m_Width && static_cast<uint32_t>(height) == m_Height)
        return;

    uint32_t oldTexture = m_Handle;
    
    // Create a new texture with the desired size
    glCreateTextures(GL_TEXTURE_2D, 1, &m_Handle);
    glBindTexture(GL_TEXTURE_2D, m_Handle);
    
    // Set texture parameters (same as the old texture)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterType);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterType);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    // Allocate storage for the new texture
    switch (m_Format)
    {
        case TextureFormat::RGBA8:
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            break;
        }
        case TextureFormat::RGBAF32:
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
            break;
        }
    }
    
    // Create a framebuffer to render the old texture into the new one (GPU-based resizing)
    uint32_t fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_Handle, 0);
    
    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cout << "Framebuffer not complete during texture resize!" << std::endl;
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(1, &m_Handle);
        m_Handle = oldTexture; // Restore old texture
        return;
    }
    
    // Set viewport to new texture size
    glViewport(0, 0, width, height);
    
    // Bind the old texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, oldTexture);
    
    // Create a temporary framebuffer for the source
    uint32_t srcFbo;
    glGenFramebuffers(1, &srcFbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, oldTexture, 0);
    
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    
    // Blit (scale) from old texture to new texture
    glBlitFramebuffer(0, 0, m_Width, m_Height,     // src rectangle
                      0, 0, width, height,         // dst rectangle
                      GL_COLOR_BUFFER_BIT,         // mask
                      filterType);                  // filter
    
    // Clean up
    glDeleteFramebuffers(1, &srcFbo);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &oldTexture);
    
    m_Width = static_cast<uint32_t>(width);
    m_Height = static_cast<uint32_t>(height);
    
    // Restore default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    std::cout << "Texture resized to: " << width << "x" << height << std::endl;
}

void Texture2D::CreateTexture(uint32_t filterType, uint32_t clampMode)
{
    glCreateTextures(GL_TEXTURE_2D, 1, &m_Handle);
    glBindTexture(GL_TEXTURE_2D, m_Handle);
    
    int error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cout << "OpenGL error after binding texture: " << error << std::endl;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filterType);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filterType);

    // S and T are relevant for 2D textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clampMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clampMode);
}

Texture2D::~Texture2D()
{
    glDeleteTextures(1, &m_Handle);
}

void Texture2D::Bind(int index)
{
    m_BindIndex = index;
    // Bind directly to the specified unit (DSA) to avoid affecting GL_ACTIVE_TEXTURE state
    glBindTextureUnit(index, m_Handle);
}

void Texture2D::Unbind()
{
    glBindTexture(GL_TEXTURE_2D, 0);
}
