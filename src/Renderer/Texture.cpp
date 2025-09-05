#include "Texture.h"

#include <stb_image.h>
#include <glad/glad.h>
#include <iostream>
#include <cassert>



Texture2D::Texture2D(const TextureCreateInfo &createInfo)
    : m_CreateInfo(createInfo)
{
    CreateTexture();
}

Texture2D::Texture2D(const TextureCreateInfo &createInfo, uint32_t hexColor)
    : m_CreateInfo(createInfo)
{
    m_Channels = 4; // RGBA format
    
    // Extract RGBA components from hex color (assuming ARGB format: 0xAARRGGBB)
    uint8_t a = (hexColor >> 24) & 0xFF; // Alpha
    uint8_t r = (hexColor >> 16) & 0xFF; // Red
    uint8_t g = (hexColor >> 8) & 0xFF;  // Green
    uint8_t b = hexColor & 0xFF;         // Blue
    
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
    
    std::cout << "Creating solid color texture: " << std::hex << hexColor << std::dec 
              << " (R:" << static_cast<int>(r) << " G:" << static_cast<int>(g) 
              << " B:" << static_cast<int>(b) << " A:" << static_cast<int>(a) << ")" 
              << " - Size: " << createInfo.width << "x" << createInfo.height << std::endl;
    
    CreateTexture();

    GLenum internalFormat = ToGLInternalFormat(createInfo.format);
    GLenum format = ToGLFormat(createInfo.format);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_CreateInfo.width, m_CreateInfo.height, 0, format, GL_UNSIGNED_BYTE, pixelData);

    delete[] pixelData;
}

Texture2D::Texture2D(const TextureCreateInfo &createInfo, void *data, size_t size)
    : m_CreateInfo(createInfo)
{
    m_Channels = 4; // Assuming RGBA format
    
    CreateTexture();

    GLenum internalFormat = ToGLInternalFormat(createInfo.format);
    GLenum format = ToGLFormat(createInfo.format);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_CreateInfo.width, m_CreateInfo.height, 0, format, GL_UNSIGNED_BYTE, data);
}

Texture2D::Texture2D(const TextureCreateInfo &createInfo, const std::string &filename)
    : m_CreateInfo(createInfo)
{
    // Don't flip HDR textures - they are typically stored in the correct orientation
    stbi_set_flip_vertically_on_load(createInfo.flip ? 1 : 0);
    
    CreateTexture();
    
    int width, height, loadedChannels;
    m_Channels = 4; // forced RGBA

    switch (createInfo.format)
    {
        case Format::RGBA8:
        {
            uint8_t *data = stbi_load(filename.c_str(), &width, &height, &loadedChannels, 4);
            if (!data)
            {
                assert(false && "Failed to load texture");
                return;
            }

            std::cout << "Loaded texture: " << filename << " - Width: " << width << ", Height: " << height << ", Channels (src->dst): " << loadedChannels << "->" << m_Channels << std::endl;

            GLenum internalFormat = ToGLInternalFormat(createInfo.format);
            GLenum format = ToGLFormat(createInfo.format);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_CreateInfo.width, m_CreateInfo.height, 0, format, GL_UNSIGNED_BYTE, data);

            if (data)
            {
                stbi_image_free(data);
            }
            break;
        }

        case Format::RGB16F:
        case Format::RGB32F:
        case Format::RGBA16F:
        case Format::RGBA32F:
        {
            // For HDR files, load as float with 3 channels (RGB) then expand to 4 (RGBA)
            float *data = stbi_loadf(filename.c_str(), &width, &height, &loadedChannels, 0);
            if (!data)
            {
                assert(false && "Failed to load HDR texture");
                return;
            }
            
            m_Channels = loadedChannels;
            
            std::cout << "Loaded HDR texture: " << filename << " - Width: " << width << ", Height: " << height 
                      << ", Channels: " << loadedChannels << std::endl;

            // Determine format based on loaded channels
            GLenum internalFormat = ToGLInternalFormat(m_CreateInfo.format);
            GLenum format = ToGLFormat(m_CreateInfo.format);

            m_CreateInfo.width = static_cast<uint32_t>(width);
            m_CreateInfo.height = static_cast<uint32_t>(height);
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_FLOAT, data);
            
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
    assert(((m_CreateInfo.width * m_CreateInfo.height * m_Channels) == size) && "Invalid image size");

    GLenum format = ToGLInternalFormat(m_CreateInfo.format);
    glBindTexture(GL_TEXTURE_2D, m_Handle);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_CreateInfo.width, m_CreateInfo.height, format, GL_UNSIGNED_BYTE, data);
}

void Texture2D::Resize(int width, int height, unsigned int filterType)
{
    // If the size is already correct, do nothing
    if (static_cast<uint32_t>(width) == m_CreateInfo.width && static_cast<uint32_t>(height) == m_CreateInfo.height)
        return;

    uint32_t oldTexture = m_Handle;
    
    CreateTexture();
    
    // Allocate storage for the new texture
    GLenum internalFormat = ToGLInternalFormat(m_CreateInfo.format);
    GLenum format = ToGLFormat(m_CreateInfo.format);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_CreateInfo.width, m_CreateInfo.height, 0, format, GL_UNSIGNED_BYTE, nullptr);

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
    glBlitFramebuffer(0, 0, m_CreateInfo.width, m_CreateInfo.height,     // src rectangle
                      0, 0, width, height,         // dst rectangle
                      GL_COLOR_BUFFER_BIT,         // mask
                      filterType);                  // filter
    
    // Clean up
    glDeleteFramebuffers(1, &srcFbo);
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &oldTexture);
    
    m_CreateInfo.width = static_cast<uint32_t>(width);
    m_CreateInfo.height = static_cast<uint32_t>(height);
    
    // Restore default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    
    std::cout << "Texture resized to: " << width << "x" << height << std::endl;
}

void Texture2D::CreateTexture()
{
    glCreateTextures(GL_TEXTURE_2D, 1, &m_Handle);
    glBindTexture(GL_TEXTURE_2D, m_Handle);
    
    int error = glGetError();
    if (error != GL_NO_ERROR)
    {
        std::cout << "OpenGL error after binding texture: " << error << std::endl;
    }

    GLenum filter = ToGLFilter(m_CreateInfo.filter);
    GLenum clamp = ToGLClampMode(m_CreateInfo.clampMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    // S and T are relevant for 2D textures
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, clamp);
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
