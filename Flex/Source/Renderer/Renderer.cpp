// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "Renderer.h"
#include "VertexArray.h"
#include "IndexBuffer.h"
#include "Texture.h"

#include <glad/glad.h>
#include <unordered_map>

namespace flex
{
    struct RendererData
    {
        std::shared_ptr<Texture2D> whiteTexture;
        std::shared_ptr<Texture2D> blackTexture;
        std::shared_ptr<Texture2D> magentaTexture;
        std::shared_ptr<Texture2D> flatNormalTexture;

        std::unordered_map<std::string, Ref<Shader>> shaderCache;
    };

    static RendererData *s_Data = nullptr;

    void Renderer::Init()
    {
        s_Data = new RendererData();
    }

    void Renderer::Shutdown()
    {
        if (s_Data)
        {
            delete s_Data;
        }
    }

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
        if (!s_Data->whiteTexture)
        {
            uint32_t color = 0xFFFFFFFF;
            TextureCreateInfo createInfo;
            createInfo.format = Format::RGBA8;
            s_Data->whiteTexture = std::make_shared<Texture2D>(createInfo, color);
        }

        return s_Data->whiteTexture;
    }

    std::shared_ptr<Texture2D> Renderer::GetBlackTexture()
    {
        if (!s_Data->blackTexture)
        {
            uint32_t color = 0xFF000000;
            TextureCreateInfo createInfo;
            createInfo.format = Format::RGBA8;
            s_Data->blackTexture = std::make_shared<Texture2D>(createInfo, color);
        }

        return s_Data->blackTexture;
    }

    std::shared_ptr<Texture2D> Renderer::GetMagentaTexture()
    {
        if (!s_Data->magentaTexture)
        {
            uint32_t color = 0xFFFF00FF;
            TextureCreateInfo createInfo;
            createInfo.format = Format::RGBA8;
            s_Data->magentaTexture = std::make_shared<Texture2D>(createInfo, color);
        }

        return s_Data->magentaTexture;
    }

    std::shared_ptr<Texture2D> Renderer::GetFlatNormalTexture()
    {
        if (!s_Data->flatNormalTexture)
        {
            // Flat normal in tangent space = (0.5, 0.5, 1.0)
            uint8_t r = static_cast<uint8_t>(0.5f * 255.0f);
            uint8_t g = static_cast<uint8_t>(0.5f * 255.0f);
            uint8_t b = 255; // 1.0
            uint8_t a = 255;
            uint32_t color = (r << 24) | (g << 16) | (b << 8) | a;
            TextureCreateInfo createInfo;
            createInfo.format = Format::RGBA8;
            s_Data->flatNormalTexture = std::make_shared<Texture2D>(createInfo, color);
        }
        return s_Data->flatNormalTexture;
    }

	Ref<Shader> Renderer::CreateShaderFromFile(const std::vector<ShaderData>& shaders, const std::string& name)
	{
        // Get loaded shader
        if (Ref<Shader> existingShader = GetShaderByName(name); existingShader)
        {
            return existingShader;
		}

        // Create new shader
		Ref<Shader> shader = CreateRef<Shader>();
		shader->CreateFromFile(shaders).Compile();
		s_Data->shaderCache[name] = shader;
        return shader;
	}

	void Renderer::RegisterShader(const Ref<Shader>& shader, const std::string& name)
	{
        if (shader && !s_Data->shaderCache.contains(name))
        {
            s_Data->shaderCache[name] = shader;
        }
	}

	Ref<Shader> Renderer::GetShaderByName(const std::string& name)
	{
        if (s_Data->shaderCache.contains(name))
			return s_Data->shaderCache[name];
		return nullptr;
	}

}