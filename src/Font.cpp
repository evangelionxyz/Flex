#include "Font.h"

#include "Renderer.h"

#include <cassert>
#include <ft2build.h>
#include <freetype/freetype.h>
#include <iostream>
#include <filesystem>

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

    for (uint32_t c = 0; c < 128; ++c)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER))
        {
            std::cerr << "Failed to load Glyph\n";
            continue;
        }

        uint32_t textureHandle;
        glGenTextures(1, &textureHandle);
        glBindTexture(GL_TEXTURE_2D, textureHandle);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
            face->glyph->bitmap.width, face->glyph->bitmap.rows,
            0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);

        // Set Texture options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        FontCharacter character =
        {
            .textureHandle = textureHandle, 
            .advance       = (uint32_t)face->glyph->advance.x,
            .size          = { face->glyph->bitmap.width,
                               face->glyph->bitmap.rows },
            .bearing       = { face->glyph->bitmap_left,
                                face->glyph->bitmap_top }
        };

        m_Characters[c] = character;
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

    // Create vertex array and vertex buffer
    m_VertexArray = std::make_shared<VertexArray>();
    m_VertexArray->Bind();

    const int vertexCount = 6;
    m_VertexBuffer = std::make_shared<VertexBuffer>(sizeof(FontVertex) * vertexCount);

	glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), (const void *)offsetof(FontVertex, position));
	
    glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), (const void *)offsetof(FontVertex, texCoord));
	
    glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(FontVertex), (const void *)offsetof(FontVertex, color));

    m_VertexArray->SetVertexBuffer(m_VertexBuffer);
}

Font::~Font()
{
    for (auto &ch : m_Characters)
    {
        glDeleteTextures(1, &ch.second.textureHandle);
    }
}

void Font::DrawText(const std::string &text, int x, int y, float scale, float color[3])
{
    m_VertexArray->Bind();

    std::string::const_iterator c;
    for (c = text.begin(); c != text.end(); ++c)
    {
        FontCharacter &ch = m_Characters[*c];

        float xPos = x + ch.bearing[0] * scale;
        float yPos = y - (ch.size[1] - ch.bearing[1]) * scale;
        float w = ch.size[0] * scale;
        float h = ch.size[1] * scale;

        FontVertex vertices[6] = 
        {
            { xPos,     yPos + h, 0.0f, 0.0f, color[0], color[1], color[2] },
            { xPos,     yPos,     0.0f, 1.0f, color[0], color[1], color[2] },
            { xPos + w, yPos,     1.0f, 1.0f, color[0], color[1], color[2] },

            { xPos,     yPos + h, 0.0f, 0.0f, color[0], color[1], color[2] },
            { xPos + w, yPos,     1.0f, 1.0f, color[0], color[1], color[2] },
            { xPos + w, yPos + h, 1.0f, 0.0f, color[0], color[1], color[2] },
        };

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, ch.textureHandle);
        m_VertexBuffer->SetData(vertices, sizeof(vertices));
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // advance cursors for next glyph
        x += (ch.advance >> 6) * scale;
    }
}

void TextRenderer::DrawText(Font *font, int x, int y, float scale, float color[3])
{

}