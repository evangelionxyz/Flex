// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef INDEX_BUFFER_H
#define INDEX_BUFFER_H

#include <cstdint>

namespace flex
{
    class IndexBuffer
    {
    public:
        IndexBuffer(const void *data, uint32_t count);

        ~IndexBuffer();

        void Bind();
        uint32_t GetCount() const { return m_Count; }
        uint32_t GetHandle() const { return m_Handle; }
    private:
        uint32_t m_Count = 0;
        uint32_t m_Handle = 0;
    };
}
#endif