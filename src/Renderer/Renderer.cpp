#include "Renderer.h"
#include "VertexArray.h"
#include "IndexBuffer.h"
#include "Texture.h"

#include <glad/glad.h>

void Renderer::Draw(std::shared_ptr<VertexArray> vertexArray, uint32_t count)
{
    vertexArray->Bind();
    glDrawArrays(GL_TRIANGLES, 0, count);
}

void Renderer::DrawIndexed(std::shared_ptr<VertexArray> vertexArray, std::shared_ptr<IndexBuffer> indexBuffer)
{
    uint32_t count = 0;
    if (indexBuffer)
    {
        indexBuffer->Bind();
        count = indexBuffer->GetCount();
    }
    else
    {
        vertexArray->GetIndexBuffer()->Bind();
        count = vertexArray->GetIndexBuffer()->GetCount();
    }

    vertexArray->Bind();
    glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, nullptr);
}

std::shared_ptr<Texture2D> Renderer::GetWhiteTexture()
{
    static std::shared_ptr<Texture2D> s_WhiteTexture;
    if (!s_WhiteTexture)
    {
        uint32_t color = 0xFFFFFFFF;
        TextureCreateInfo createInfo;
        createInfo.format = Format::RGBA8;
        s_WhiteTexture = std::make_shared<Texture2D>(createInfo, color);
    }

    return s_WhiteTexture;
}

std::shared_ptr<Texture2D> Renderer::GetBlackTexture()
{
    static std::shared_ptr<Texture2D> s_BlackTexture;
    if (!s_BlackTexture)
    {
        uint32_t color = 0xFF000000;
        TextureCreateInfo createInfo;
        createInfo.format = Format::RGBA8;
        s_BlackTexture = std::make_shared<Texture2D>(createInfo, color);
    }

    return s_BlackTexture;
}

std::shared_ptr<Texture2D> Renderer::GetMagentaTexture()
{
    static std::shared_ptr<Texture2D> s_MagentaTexture;
    if (!s_MagentaTexture)
    {
        uint32_t color = 0xFFFF00FF;
        TextureCreateInfo createInfo;
        createInfo.format = Format::RGBA8;
        s_MagentaTexture = std::make_shared<Texture2D>(createInfo, color);
    }

    return s_MagentaTexture;
}

std::shared_ptr<Texture2D> Renderer::GetFlatNormalTexture()
{
    static std::shared_ptr<Texture2D> s_FlatNormalTexture;
    if (!s_FlatNormalTexture)
    {
        // Flat normal in tangent space = (0.5, 0.5, 1.0)
        uint8_t r = static_cast<uint8_t>(0.5f * 255.0f);
        uint8_t g = static_cast<uint8_t>(0.5f * 255.0f);
        uint8_t b = 255; // 1.0
        uint8_t a = 255;
        uint32_t color = (r << 24) | (g << 16) | (b << 8) | a;
        TextureCreateInfo createInfo;
        createInfo.format = Format::RGBA8;
        s_FlatNormalTexture = std::make_shared<Texture2D>(createInfo, color);
    }
    return s_FlatNormalTexture;
}