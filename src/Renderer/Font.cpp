#include "Font.h"

#include "Renderer.h"
#include "Shader.h"

#include <cassert>
#include <ft2build.h>
#include <freetype/freetype.h>
#include <iostream>
#include <filesystem>
#include <array>

Font::Font(const std::string &filename, int fontSize)
    : m_FontSize(fontSize)
{
    assert(std::filesystem::exists(filename) && "Font file dould not found");

    // Init Freetype Library
    FT_Library ft;
    if (FT_Init_FreeType(&ft))
    {
        assert(false && "Failed to initialize FreeType library");
        return;
    }

    // Load Font
    FT_Face face;
    if (FT_New_Face(ft, filename.c_str(), 0, &face))
    {
        assert(false && "Failed to load font");
        return;
    }

    FT_Set_Pixel_Sizes(face, 0, fontSize);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    uint32_t atlasWidth = 0;
    uint32_t atlasHeight = 0;

    // First pass, calculate the required atlas dimensions
    for (uint32_t c = 0; c < 128; ++c)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            continue;
        }

        atlasWidth += face->glyph->bitmap.width;
        atlasHeight = std::max(atlasHeight, face->glyph->bitmap.rows);
    }

    // Create atlas texture
    glGenTextures(1, &m_TextureHandle);
    glBindTexture(GL_TEXTURE_2D, m_TextureHandle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, atlasWidth, atlasHeight, 0,
                 GL_RED, GL_UNSIGNED_BYTE, nullptr);

    // Set texture options
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Second pass, load glyphs and upload them to the atlas texture
    uint32_t xOffset = 0;
    for (uint32_t c = 0; c < 128; ++c)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            continue;
        }

        // Get glyph dimensions
        uint32_t glyphWidth = face->glyph->bitmap.width;
        uint32_t glyphHeight = face->glyph->bitmap.rows;

        // Upload the glyph bitmap to the atlas texture using glTexSubImage2D
        if (glyphWidth > 0 && glyphHeight > 0)
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, xOffset, 0,
                            glyphWidth, glyphHeight, GL_RED, GL_UNSIGNED_BYTE,
                            face->glyph->bitmap.buffer);
        }

        // Calculate uv coordinates
        glm::vec2 uvBL = {(float)xOffset / atlasWidth, 0.0f};
        glm::vec2 uvTR = {(float)(xOffset + glyphWidth) / atlasWidth,
                          (float)glyphHeight / atlasHeight};

        FontCharacter character =
            {
                .advance = (uint32_t)face->glyph->advance.x,
                .uvBottomLeft = uvBL,
                .uvTopRight = uvTR,
                .size = {face->glyph->bitmap.width,
                         face->glyph->bitmap.rows},
                .bearing = {face->glyph->bitmap_left,
                            face->glyph->bitmap_top}};

        m_Characters[c] = character;

        // Advance the x offset for the next character
        xOffset += glyphWidth;
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}

Font::~Font()
{
    glDeleteTextures(1, &m_TextureHandle);
}

// ------------------------
// Text Renderer

struct TextRendererData
{
    const uint32_t MAX_VERTICES = 1024 * 3;
    const uint32_t MAX_INDICES = 1024 * 6;

    const uint32_t MAX_FONTS = 32;
    uint32_t vertexCount = 0;
    std::array<Font *, 32> fonts;

    std::shared_ptr<VertexArray> vertexArray;
    std::shared_ptr<VertexBuffer> vertexBuffer;
    uint32_t fontTextureIndex = 0;

    FontVertex *vertexPointer = nullptr;
    FontVertex *vertexPointerBase = nullptr;
    std::shared_ptr<Shader> shader;
};

static TextRendererData s_TextData;

void TextRenderer::Init()
{
    s_TextData.shader = std::make_shared<Shader>();
    s_TextData.shader->AddFromFile("resources/shaders/text.vertex.glsl", GL_VERTEX_SHADER)
        .AddFromFile("resources/shaders/text.frag.glsl", GL_FRAGMENT_SHADER)
        .Compile();

    s_TextData.fonts = {nullptr};
    s_TextData.vertexPointerBase = new FontVertex[s_TextData.MAX_VERTICES];

    s_TextData.vertexArray = std::make_shared<VertexArray>();
    s_TextData.vertexBuffer = std::make_shared<VertexBuffer>(sizeof(FontVertex) * s_TextData.MAX_VERTICES);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), (const void *)offsetof(FontVertex, position));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), (const void *)offsetof(FontVertex, texCoord));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(FontVertex), (const void *)offsetof(FontVertex, color));

    glEnableVertexAttribArray(3);
    // Use glVertexAttribIPointer for integers
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(FontVertex), (const void *)offsetof(FontVertex, textureIndex));

    s_TextData.vertexArray->SetVertexBuffer(s_TextData.vertexBuffer);
}

void TextRenderer::Shutdown()
{
    if (s_TextData.vertexPointerBase)
        delete[] s_TextData.vertexPointerBase;
}

void TextRenderer::Begin(const glm::mat4 &viewProjection)
{
    s_TextData.vertexPointer = s_TextData.vertexPointerBase;
    s_TextData.vertexCount = 0;

    s_TextData.shader->Use();
    s_TextData.shader->SetUniform("viewProjection", viewProjection);
}

void TextRenderer::End()
{
    if (s_TextData.vertexCount)
    {
        s_TextData.vertexArray->Bind();

        uint64_t byteSize = (uint64_t)((uint8_t *)(s_TextData.vertexPointer) - (uint8_t *)(s_TextData.vertexPointerBase));
        s_TextData.vertexBuffer->SetData(s_TextData.vertexPointerBase, byteSize);

        for (uint32_t i = 0; i < s_TextData.fontTextureIndex; ++i)
        {
            if (s_TextData.fonts[i])
            {
                glBindTextureUnit(i, s_TextData.fonts[i]->GetTextureHandle());
            }
        }

        glDrawArrays(GL_TRIANGLES, 0, s_TextData.vertexCount);
    }
}

void TextRenderer::DrawText(Font *font, const std::string &text, const  int x, const int y, const float scale, const glm::vec3 &color, const TextParameter &params)
{
    int textureIndex = GetFontTextureIndex(font);

    // load store new font
    if (textureIndex == -1)
    {
        textureIndex = s_TextData.fontTextureIndex++;
        s_TextData.fonts[textureIndex] = font;
    }

    float POSITION_X = x;
    float POSITION_Y = y;

    for (size_t i = 0; i < text.size(); ++i)
    {
        char c = text[i];
        FontCharacter &ch = font->GetCharacters()[c];

        // Return
        if (c == '\r')
        {
            continue;
        }

        // New Character
        if (c == '\n')
        {
            POSITION_X = x;
            POSITION_Y -= scale * (float)font->GetFontSize() + params.lineSpacing;
            continue;
        }

        if (c == ' ')
        {
            POSITION_X += params.kerning + scale;
        }

        float xPos = POSITION_X + ch.bearing.x * scale;
        float yPos = POSITION_Y - (ch.size.y - ch.bearing.y) * scale;
        float w = ch.size.x * scale;
        float h = ch.size.y * scale;

        // Define the 4 corners of the quad
        glm::vec2 pos_bl = {xPos, yPos};
        glm::vec2 pos_br = {xPos + w, yPos};
        glm::vec2 pos_tr = {xPos + w, yPos + h};
        glm::vec2 pos_tl = {xPos, yPos + h};

        // Define the 4 corresponding UV coordinates from the atlas
        // glm::vec2 uv_bl = { ch.uvBottomLeft.x, ch.uvBottomLeft.y };
        // glm::vec2 uv_br = { ch.uvTopRight.x,   ch.uvBottomLeft.y };
        // glm::vec2 uv_tr = { ch.uvTopRight.x,   ch.uvTopRight.y };
        // glm::vec2 uv_tl = { ch.uvBottomLeft.x, ch.uvTopRight.y };

        // Define the 4 corresponding UV coordinates, swapping the Y to fix the orientation
        glm::vec2 uv_bl = {ch.uvBottomLeft.x, ch.uvTopRight.y};
        glm::vec2 uv_br = {ch.uvTopRight.x, ch.uvTopRight.y};
        glm::vec2 uv_tr = {ch.uvTopRight.x, ch.uvBottomLeft.y};
        glm::vec2 uv_tl = {ch.uvBottomLeft.x, ch.uvBottomLeft.y};

        // Triangle 1
        s_TextData.vertexPointer->position = pos_bl;
        s_TextData.vertexPointer->texCoord = uv_bl;
        s_TextData.vertexPointer->color = color;
        s_TextData.vertexPointer->textureIndex = (float)textureIndex;
        s_TextData.vertexPointer++;

        s_TextData.vertexPointer->position = pos_br;
        s_TextData.vertexPointer->texCoord = uv_br;
        s_TextData.vertexPointer->color = color;
        s_TextData.vertexPointer->textureIndex = (float)textureIndex;
        s_TextData.vertexPointer++;

        s_TextData.vertexPointer->position = pos_tr;
        s_TextData.vertexPointer->texCoord = uv_tr;
        s_TextData.vertexPointer->color = color;
        s_TextData.vertexPointer++;

        // Triangle 2
        s_TextData.vertexPointer->position = pos_bl;
        s_TextData.vertexPointer->texCoord = uv_bl;
        s_TextData.vertexPointer->color = color;
        s_TextData.vertexPointer->textureIndex = (float)textureIndex;
        s_TextData.vertexPointer++;

        s_TextData.vertexPointer->position = pos_tr;
        s_TextData.vertexPointer->texCoord = uv_tr;
        s_TextData.vertexPointer->color = color;
        s_TextData.vertexPointer->textureIndex = (float)textureIndex;
        s_TextData.vertexPointer++;

        s_TextData.vertexPointer->position = pos_tl;
        s_TextData.vertexPointer->texCoord = uv_tl;
        s_TextData.vertexPointer->color = color;
        s_TextData.vertexPointer->textureIndex = (float)textureIndex;
        s_TextData.vertexPointer++;

        s_TextData.vertexCount += 6;

        // Advance cursor for next glyph
        POSITION_X += (ch.advance >> 6) * scale; // Note: FreeType's advance is 1/64 pixels
    }
}

int TextRenderer::GetFontTextureIndex(Font *font)
{
    for (int i = 0; i < s_TextData.MAX_FONTS; ++i)
    {
        if (s_TextData.fonts[i] == font)
            return i;
    }
    return -1;
}
