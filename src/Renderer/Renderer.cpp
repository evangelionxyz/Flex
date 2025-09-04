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
        TextureCreateInfo createInfo;
        createInfo.format = TextureFormat::RGBA8;
        createInfo.hexColor = 0xFFFFFFFF;
        s_WhiteTexture = std::make_shared<Texture2D>(createInfo);
    }

    return s_WhiteTexture;
}

std::shared_ptr<Texture2D> Renderer::GetBlackTexture()
{
    static std::shared_ptr<Texture2D> s_BlackTexture;
    if (!s_BlackTexture)
    {
        TextureCreateInfo createInfo;
        createInfo.format = TextureFormat::RGBA8;
        createInfo.hexColor = 0x000000FF;
        s_BlackTexture = std::make_shared<Texture2D>(createInfo);
    }

    return s_BlackTexture;
}

std::shared_ptr<Texture2D> Renderer::GetMagentaTexture()
{
    static std::shared_ptr<Texture2D> s_MagentaTexture;
    if (!s_MagentaTexture)
    {
        TextureCreateInfo createInfo;
        createInfo.format = TextureFormat::RGBA8;
        createInfo.hexColor = 0xFF00FFFF;
        s_MagentaTexture = std::make_shared<Texture2D>(createInfo);
    }

    return s_MagentaTexture;
}