// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef BUFFER_H
#define BUFFER_H

#include <cstring>
#include <cstdint>

namespace flex
{
    class Buffer
    {
    public:
        Buffer()
            : m_Data(nullptr), m_Size(0)
        {
        }

        Buffer(void *data, size_t size)
            : m_Data(data), m_Size(size)
        {
        }

        ~Buffer()
        {
            if (m_Data)
                free(m_Data);
            m_Data = nullptr;
            m_Size = 0;
        }

        void Allocate(size_t size)
        {
            m_Data = (uint8_t *)malloc(size);
            m_Size = size;
        }

        size_t Size() const { return m_Size; }
        uint8_t *Data() { return m_Data; }

        bool operator() { return m_Size != 0 && m_Data != nullptr; }
    private:
        uint8_t *m_Data = nullptr;
        size_t m_Size = 0;
    };
}

#endif