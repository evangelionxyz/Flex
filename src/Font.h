#pragma once

#include "VertexArray.h"
#include "VertexBuffer.h"
#include <string>
#include <cstdint>
#include <map>

struct FontVertex
{
    float position[2]; // x, y
    float texCoord[2]; // u, v
    float color[3];    // r, g, b
};

struct FontCharacter
{
    uint32_t textureHandle;
    uint32_t advance;
    int size[2];
    int bearing[2];
};

class Font
{
public:
    Font(const std::string &filename, int fontSize = 16);
    ~Font();

    void DrawText(const std::string &text, int x, int y, float scale, float color[3]);
    int GetFontSize() const { return m_FontSize; }
    
private:
    std::map<char, FontCharacter> m_Characters;
    std::shared_ptr<VertexArray> m_VertexArray;
    std::shared_ptr<VertexBuffer> m_VertexBuffer;
    int m_FontSize;
};

class TextRenderer
{
public:
    static void DrawText(Font *font, int x, int y, float scale, float color[3]);
};