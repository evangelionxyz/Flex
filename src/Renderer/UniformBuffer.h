#pragma once

#include <memory>
#include <stdint.h>

class UniformBuffer
{
public:
    UniformBuffer(size_t size, uint32_t index);
    ~UniformBuffer();

    void SetData(void *data, size_t size, size_t offset = 0);

    void Bind();

    static std::shared_ptr<UniformBuffer> Create(size_t size, uint32_t index = 0);

private:
    uint32_t m_Handle;
    uint32_t m_BindIndex;
};