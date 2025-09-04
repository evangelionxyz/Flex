#include "UniformBuffer.h"
#include <glad/glad.h>

#include <cassert>

UniformBuffer::UniformBuffer(size_t size, uint32_t index)
    : m_BindIndex(index)
{
    glCreateBuffers(1, &m_Handle);
    glBindBuffer(GL_UNIFORM_BUFFER, m_Handle);
    glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, m_BindIndex, m_Handle);

    int err = glGetError();
    assert(err == GL_NO_ERROR);
}

UniformBuffer::~UniformBuffer()
{
    glDeleteBuffers(1, &m_Handle);
}

void UniformBuffer::SetData(void *data, size_t size, size_t offset)
{
    glBindBuffer(GL_UNIFORM_BUFFER, m_Handle);
    glBindBufferBase(GL_UNIFORM_BUFFER, m_BindIndex, m_Handle);
    glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
}

void UniformBuffer::Bind()
{
    glBindBuffer(GL_UNIFORM_BUFFER, m_Handle);
    glBindBufferBase(GL_UNIFORM_BUFFER, m_BindIndex, m_Handle);
}

std::shared_ptr<UniformBuffer> UniformBuffer::Create(size_t size, uint32_t index)
{
    return std::make_shared<UniformBuffer>(size, index);
}