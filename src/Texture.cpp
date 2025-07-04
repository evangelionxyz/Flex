#include "Texture.h"

#include "stb_image.h"
#include <glad/glad.h>
#include <iostream>
#include <cassert>

static GLenum GetFormatFromChannel8BIT(int channels)
{
    switch (channels)
    {
    case 1: return GL_R8;
    case 2: return GL_RG8;
    case 3: return GL_RGB8;
    case 4: return GL_RGBA8;
    default: return GL_RGBA8; // fallback
    }
}

static GLenum GetFormatFromChannel(int channels)
{
    switch (channels)
    {
    case 1: return GL_RED;
    case 2: return GL_RG;
    case 3: return GL_RGB;
    case 4: return GL_RGBA;
    default: return GL_RGBA; // fallback
    }
}

Texture2D::Texture2D(const std::string &filename)
{
    stbi_set_flip_vertically_on_load(1);

    int width, height, channels;
    uint8_t *data = stbi_load(filename.c_str(), &width, &height, &channels, 0);
    if (!data)
    {
        assert(false && "Failed to load texture");
        return;
    }

    std::cout << "Loaded texture: " << filename << " - Width: " << width << ", Height: " << height << ", Channels: " << channels << std::endl;

    GLenum internalFormat = GetFormatFromChannel8BIT(channels);
    GLenum format = GetFormatFromChannel(channels);

    glGenTextures(1, &m_Handle);
    glBindTexture(GL_TEXTURE_2D, m_Handle);
    
    int error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "OpenGL error after binding texture: " << error << std::endl;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cout << "OpenGL error after uploading texture data: " << error << std::endl;
    }

    if (data)
    {
        stbi_image_free(data);
    }
}

Texture2D::~Texture2D()
{
    glDeleteTextures(1, &m_Handle);
}

void Texture2D::Bind(int index)
{
    glActiveTexture(GL_TEXTURE0 + index);
    glBindTexture(GL_TEXTURE_2D, m_Handle);
}
