#pragma once

#include <memory>

class IndexBuffer;
class VertexArray;
class Texture2D;

class Renderer
{
public:
    static void Draw(std::shared_ptr<VertexArray> vertexArray, uint32_t count);
    static void DrawIndexed(std::shared_ptr<VertexArray> vertexArray, std::shared_ptr<IndexBuffer> indexBuffer = nullptr);

    static std::shared_ptr<Texture2D> GetWhiteTexture();
    static std::shared_ptr<Texture2D> GetBlackTexture();
    static std::shared_ptr<Texture2D> GetMagentaTexture();
};
