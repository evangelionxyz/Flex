// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef FONT_H
#define FONT_H

#include <string>
#include <cstdint>
#include <unordered_map>

#include <glm/glm.hpp>

#include <msdf-atlas-gen.h>
#include <msdfgen.h>

namespace flex
{

    struct FontVertex
    {
        glm::vec3 position; // x, y
        glm::vec3 color;    // r, g, b
        glm::vec2 uv; // u, v
        int textureIndex;
    };

    class Font
    {
    public:
        Font(const std::string &filename, int fontSize = 16);
        ~Font();

        int GetFontSize() const { return m_FontSize; }
        uint32_t GetTextureHandle() const { return m_TextureHandle; }
        
        const msdf_atlas::FontGeometry &GetGeometry() const { return m_FontGeometry; }
        const std::vector<msdf_atlas::GlyphGeometry> &GetGlyphs() { return m_Glyphs; }
        const glm::vec2 &GetAtlasSize() { return m_AtlasSize; }

    private:
        msdf_atlas::FontGeometry m_FontGeometry;
        std::vector<msdf_atlas::GlyphGeometry> m_Glyphs;
        glm::vec2 m_AtlasSize;
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

        static void DrawString(Font *font, const std::string &text, const glm::mat4 &transform, const glm::vec3 &color, const TextParameter &params);
        static int GetFontTextureIndex(Font *font);

    };
}

#endif