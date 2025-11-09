// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "VertexArray.h"
#include "IndexBuffer.h"
#include "VertexBuffer.h"

#include <glad/glad.h>

namespace flex
{
    VertexArray::VertexArray()
    {
        glCreateVertexArrays(1, &m_Handle);
        glBindVertexArray(m_Handle);

        assert(m_Handle != 0 && "Failed to create Vertex array!");
    }
    
    void VertexArray::SetVertexBuffer(std::shared_ptr<VertexBuffer> vertexBuffer)
    {
        m_VertexBuffer = vertexBuffer;
    }
    
    void VertexArray::SetIndexBuffer(std::shared_ptr<IndexBuffer> indexBuffer)
    {
        m_IndexBuffer = indexBuffer;
    }
    
    VertexArray::~VertexArray()
    {
        m_VertexBuffer = nullptr;
        m_IndexBuffer = nullptr;

        glDeleteVertexArrays(1, &m_Handle);
    }

    void VertexArray::Bind()
    {
        glBindVertexArray(m_Handle);
    }
}