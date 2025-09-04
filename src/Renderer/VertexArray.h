#pragma once
#include <memory>
#include <cstdint>

class VertexBuffer;
class IndexBuffer;

class VertexArray
{
public:
    VertexArray();
    ~VertexArray();
    
    void SetVertexBuffer(std::shared_ptr<VertexBuffer> vertexBuffer);
    void SetIndexBuffer(std::shared_ptr<IndexBuffer> indexBuffer);
    
    void Bind();
    
    std::shared_ptr<IndexBuffer> GetIndexBuffer() { return m_IndexBuffer; }
    uint32_t GetHandle() const { return m_Handle; }

private:
    std::shared_ptr<VertexBuffer> m_VertexBuffer;
    std::shared_ptr<IndexBuffer> m_IndexBuffer;
    uint32_t m_Handle = 0;
};