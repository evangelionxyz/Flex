#include "IndexBuffer.h"

#include <glad/glad.h>

IndexBuffer::IndexBuffer(const void *data, uint64_t size, uint32_t count)
    : m_Count(count)
{
    glGenBuffers(1, &m_Handle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Handle);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
}

IndexBuffer::~IndexBuffer()
{
    glDeleteBuffers(1, &m_Handle);
}

void IndexBuffer::Bind()
{
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_Handle);
}