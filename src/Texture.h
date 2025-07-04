#pragma once

#include <cstdint>
#include <string>

class Texture2D
{
public:
    Texture2D(const std::string &filename);
    ~Texture2D();
    
    void Bind(int index);

    uint32_t GetHandle() const { return m_Handle; }
private:
    uint32_t m_Handle;
};