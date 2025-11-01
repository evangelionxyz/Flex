// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef VERTEX_ARRAY_H
#define VERTEX_ARRYA_H

#include <memory>
#include <cstdint>

namespace flex
{
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
}

#endif