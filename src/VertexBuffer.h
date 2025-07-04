#pragma once

#include <cstdint>
#include <cassert>

#include <glad/glad.h>

enum class Format
{
    R,
    RG,
    RGB,
    RGBA
};

static uint8_t GLGetElementCount(GLenum type)
{
    switch (type)
    {
        case GL_R: return 1;
        case GL_RG: return 2;
        case GL_RGB: return 3;
        case GL_RGBA: return 4;
    }

    assert(false && "Invalid type");
    return 0;
}

struct BufferElement
{

    void CalculateStride()
    {

    }

private:
    uint32_t m_Stride;
};

class VertexBuffer
{
public:
    VertexBuffer(const void *data, uint64_t size);
    VertexBuffer(uint64_t size);

    ~VertexBuffer();

    void SetData(const void *data, uint64_t size, uint64_t offset = 0);
    void Bind();

    uint32_t GetHandle() const { return m_Handle; }

private:
    uint32_t m_Handle = 0;
};