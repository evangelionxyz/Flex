// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "VertexBuffer.h"

#include <glad/glad.h>

namespace flex
{
    VertexBuffer::VertexBuffer(const void *data, uint64_t size)
    {
        glCreateBuffers(1, &m_Handle);
        glBindBuffer(GL_ARRAY_BUFFER, m_Handle);
        glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);
    }

    VertexBuffer::VertexBuffer(uint64_t size)
    {
        glCreateBuffers(1, &m_Handle);
        glBindBuffer(GL_ARRAY_BUFFER, m_Handle);
        glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
    }

    VertexBuffer::~VertexBuffer()
    {
        glDeleteBuffers(1, &m_Handle);
    }

    void VertexBuffer::SetAttributes(std::initializer_list<VertexAttribute> attributes, uint32_t stride)
    {
        int index = 0;
        int totalElementBytes = 0;
        for (auto it = attributes.begin(); it != attributes.end(); ++it)
        {
            uint8_t elementCount = GetVertexElementCount(it->type);
            GLenum glElementType = GetGLVertexElementType(it->type);

            switch (it->type)
            {
                case VertexAttribType::INT:
                case VertexAttribType::VECTOR_INT_2:
                case VertexAttribType::VECTOR_INT_3:
                case VertexAttribType::VECTOR_INT_4:
                {
                    glVertexAttribIPointer(index, elementCount, glElementType, stride,
                        (const void *)(intptr_t)(totalElementBytes));

                    totalElementBytes += elementCount * sizeof(int);
                    break;
                }
                case VertexAttribType::FLOAT:
                case VertexAttribType::VECTOR_FLOAT_2:
                case VertexAttribType::VECTOR_FLOAT_3:
                case VertexAttribType::VECTOR_FLOAT_4:
                {
                    glVertexAttribPointer(index, elementCount, glElementType, it->normalized, stride,
                        (const void *)(intptr_t)(totalElementBytes));

                    totalElementBytes += elementCount * sizeof(float);
                    break;
                }
                case VertexAttribType::MATRIX_FLOAT_3X3:
                case VertexAttribType::MATRIX_FLOAT_4X4:
                {
                    break;
                }
            }

            glEnableVertexAttribArray(index);
            index++;
        }
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
}
