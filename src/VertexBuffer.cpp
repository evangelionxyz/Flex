#include "VertexBuffer.h"

#include <glad/glad.h>

VertexBuffer::VertexBuffer(const void *data, uint64_t size)
{
    glGenBuffers(1, &m_Handle);
    glBindBuffer(GL_ARRAY_BUFFER, m_Handle);
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
}

VertexBuffer::VertexBuffer(uint64_t size)
{
    glGenBuffers(1, &m_Handle);
    glBindBuffer(GL_ARRAY_BUFFER, m_Handle);
    glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
}

VertexBuffer::~VertexBuffer()
{
    glDeleteBuffers(1, &m_Handle);
}

void VertexBuffer::SetData(const void *data, uint64_t size, uint64_t offset)
{
    glBindBuffer(GL_ARRAY_BUFFER, m_Handle);
    glBufferSubData(GL_ARRAY_BUFFER, offset, size, data);
}

void VertexBuffer::Bind()
{
    glBindBuffer(GL_ARRAY_BUFFER, m_Handle);
}
