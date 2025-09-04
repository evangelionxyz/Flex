#pragma once

#include <cstdint>
#include <cassert>
#include <initializer_list>

#include <glad/glad.h>

enum class VertexAttribType
{
    INT,
    VECTOR_INT_2,
    VECTOR_INT_3,
    VECTOR_INT_4,

    FLOAT,
    VECTOR_FLOAT_2,
    VECTOR_FLOAT_3,
    VECTOR_FLOAT_4,

    MATRIX_FLOAT_3X3,
    MATRIX_FLOAT_4X4,
};

static uint8_t GetVertexElementCount(VertexAttribType type)
{
    switch (type)
    {
        case VertexAttribType::INT: return 1;
        case VertexAttribType::VECTOR_INT_2: return 2;
        case VertexAttribType::VECTOR_INT_3: return 3;
        case VertexAttribType::VECTOR_INT_4: return 4;

        case VertexAttribType::FLOAT: return 1;
        case VertexAttribType::VECTOR_FLOAT_2: return 2;
        case VertexAttribType::VECTOR_FLOAT_3: return 3;
        case VertexAttribType::VECTOR_FLOAT_4: return 4;

        case VertexAttribType::MATRIX_FLOAT_3X3: return 3;
        case VertexAttribType::MATRIX_FLOAT_4X4: return 4;
    }

    assert(false && "Invalid type");
    return 0;
}

static GLenum GetGLVertexElementType(VertexAttribType type)
{
    switch (type)
    {
        case VertexAttribType::INT:
        case VertexAttribType::VECTOR_INT_2:
        case VertexAttribType::VECTOR_INT_3:
        case VertexAttribType::VECTOR_INT_4: return GL_INT;

        case VertexAttribType::FLOAT:
        case VertexAttribType::VECTOR_FLOAT_2:
        case VertexAttribType::VECTOR_FLOAT_3:
        case VertexAttribType::VECTOR_FLOAT_4:
        case VertexAttribType::MATRIX_FLOAT_3X3:
        case VertexAttribType::MATRIX_FLOAT_4X4: return GL_FLOAT;
    }

    assert(false && "Invalid type");
    return 0;
}

struct VertexAttribute
{
    VertexAttribType type;
    bool normalized = false;
};

class VertexBuffer
{
public:
    VertexBuffer(const void *data, uint64_t size);
    VertexBuffer(uint64_t size);

    void SetAttributes(std::initializer_list<VertexAttribute> attributes, uint32_t stride);

    ~VertexBuffer();

    void SetData(const void *data, uint64_t size, uint64_t offset = 0);
    void Bind();

    uint32_t GetHandle() const { return m_Handle; }

private:
    uint32_t m_Handle = 0;
};