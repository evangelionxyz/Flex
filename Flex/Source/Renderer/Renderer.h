// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef RENDERER_H
#define RENDERER_H

#include "Core/Types.h"
#include "Shader.h"

#include <string>

namespace flex
{
    class IndexBuffer;
    class VertexArray;
    class Texture2D;

    class Renderer
    {
    public:
        static void Init();
        static void Shutdown();
        
        static void Draw(std::shared_ptr<VertexArray> vertexArray, uint32_t count);
        static void DrawIndexed(std::shared_ptr<VertexArray> vertexArray, std::shared_ptr<IndexBuffer> indexBuffer = nullptr);

        static std::shared_ptr<Texture2D> GetWhiteTexture();
        static std::shared_ptr<Texture2D> GetBlackTexture();
        static std::shared_ptr<Texture2D> GetMagentaTexture();

        // Flat normal texture (0.5,0.5,1.0) used as a neutral normal map fallback
        static std::shared_ptr<Texture2D> GetFlatNormalTexture();

		static Ref<Shader> CreateShaderFromFile(const std::vector<ShaderData> &shaders, const std::string& name);
        static void RegisterShader(const Ref<Shader> &shader, const std::string &name);
		static Ref<Shader> GetShaderByName(const std::string& name);
    };
}

#endif
