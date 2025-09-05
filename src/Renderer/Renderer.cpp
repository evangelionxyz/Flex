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
        uint32_t color = 0x000000FF;
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
        uint32_t color = 0xFF00FFFF;
        TextureCreateInfo createInfo;
        createInfo.format = Format::RGBA8;
        s_MagentaTexture = std::make_shared<Texture2D>(createInfo, color);
    }

    return s_MagentaTexture;
}