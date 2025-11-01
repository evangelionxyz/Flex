// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef UNIFORM_BUFFER_H
#define UNIFORM_BUFFER_H

#include <memory>
#include <stdint.h>

namespace flex
{
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
}

#endif