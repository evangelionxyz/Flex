#pragma once

#include "VertexArray.h"
#include "VertexBuffer.h"
#include <string>
#include <cstdint>
#include <map>

#include <glm/glm.hpp>

struct FontVertex
{
    glm::vec2 position; // x, y
    glm::vec2 texCoord; // u, v
    glm::vec3 color;    // r, g, b
    float textureIndex;
};

struct FontCharacter
{
    uint32_t advance;
    glm::vec2 uvBottomLeft;
    glm::vec2 uvTopRight;
    glm::ivec2 size;
    glm::ivec2 bearing;
};

class Font
{
public:
    Font(const std::string &filename, int fontSize = 16);
    ~Font();

    int GetFontSize() const { return m_FontSize; }
    uint32_t GetTextureHandle() const { return m_TextureHandle; }
    std::map<char, FontCharacter> GetCharacters() const { return m_Characters; }
    
private:
    std::map<char, FontCharacter> m_Characters;
    uint32_t m_TextureHandle;
    int m_FontSize;
};

struct TextParameter
{
    float lineSpacing = 1.0f;
    float kerning = 0.0f;
};

class TextRenderer
{
public:
    static void Init();
    static void Shutdown();

    static void Begin(const glm::mat4 &viewProjection);
    static void End();

    static void DrawText(Font *font, const std::string &text, const int x, const int y, const float scale, const glm::vec3 &color, const TextParameter &params);
    static int GetFontTextureIndex(Font *font);

};