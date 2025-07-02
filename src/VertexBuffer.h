#pragma once

#include <cstdint>

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