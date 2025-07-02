#include "VertexArray.h"
#include "IndexBuffer.h"
#include "VertexBuffer.h"

#include <glad/glad.h>

VertexArray::VertexArray()
{
    glGenVertexArrays(1, &m_Handle);
    glBindVertexArray(m_Handle);
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
    m_VertexBuffer->Bind();
    glBindVertexArray(m_Handle);
}