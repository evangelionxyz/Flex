#include "Font.h"

#include "Renderer.h"
#include "Shader.h"

#include "VertexArray.h"
#include "IndexBuffer.h"
#include "VertexBuffer.h"

#include <cassert>
#include <ft2build.h>
#include <freetype/freetype.h>
#include <iostream>
#include <filesystem>
#include <array>

Font::Font(const std::string &filename, int fontSize)
    : m_FontSize(fontSize)
{
    assert(std::filesystem::exists(filename) && "Font file could not be found");

    msdfgen::FreetypeHandle *ft = msdfgen::initializeFreetype();
    if (!ft)
    {
        assert(false && "Failed to initialize FreeType library");
        return;
    }

    msdfgen::FontHandle *font = msdfgen::loadFont(ft, filename.c_str());
    if (!font)
    {
        assert(false && "Failed to load font");
        msdfgen::deinitializeFreetype(ft);
        return;
    }

    struct CharsetRange
    {
        uint32_t Begin, End;
    };

    // From imgui_draw.cpp
    static const CharsetRange charsetRanges[] =
    {
        {0x0020, 0x00FF}
    };
    msdf_atlas::Charset charset;
    for (CharsetRange range : charsetRanges)
    {
        for (uint32_t c = range.Begin; c <= range.End; c++)
            charset.add(c);
    }
    m_FontGeometry = msdf_atlas::FontGeometry(&m_Glyphs);
    const double fontScale = 1.0;
    const int glyphsLoaded = m_FontGeometry.loadCharset(font, fontScale, charset);
    printf("Font loaded %d glyphs\n", glyphsLoaded);

    const double emSize = 40.0;
    const int width = 1024;
    const int height = 1024;

    m_AtlasSize.x = (float)width;
    m_AtlasSize.y = (float)height;

    msdf_atlas::TightAtlasPacker atlasPacker;
    atlasPacker.setDimensions(width, height);
    atlasPacker.setPadding(1);
    atlasPacker.setPixelRange(4.0);
    atlasPacker.setScale(emSize);
    int remaining = atlasPacker.pack(m_Glyphs.data(), m_Glyphs.size());
    assert(remaining == 0);

#define DEFAULT_ANGLE_THRESHOLD 3.0
#define LCG_MULTIPLIER 6364136223846793005ull
#define LCG_INCREMENT 1442695040888963407ull
#define THREAD_COUNT 8

    uint64_t coloringSeed = 0;
    bool expesiveColoring = false;
    if (expesiveColoring)
    {
            msdf_atlas::Workload([&glyphs = m_Glyphs, &coloringSeed](int i, int threadNo)->bool
			{
				unsigned long long glyphSeed = (LCG_MULTIPLIER * (coloringSeed ^ i) + LCG_INCREMENT) * !!coloringSeed;
				glyphs[i].edgeColoring(msdfgen::edgeColoringInkTrap, DEFAULT_ANGLE_THRESHOLD, glyphSeed);
				return true;
			},
			static_cast<int>(m_Glyphs.size())).finish(THREAD_COUNT);
    }
    else
    {
        unsigned long long glyphSeed = coloringSeed;
        for (msdf_atlas::GlyphGeometry &glyph : m_Glyphs)
        {
            glyphSeed *= LCG_MULTIPLIER;
            glyph.edgeColoring(msdfgen::edgeColoringInkTrap, DEFAULT_ANGLE_THRESHOLD, glyphSeed);
        }
    }

    msdf_atlas::ImmediateAtlasGenerator<float, 3, msdf_atlas::msdfGenerator, msdf_atlas::BitmapAtlasStorage<float, 3>> generator(width, height);
    msdf_atlas::GeneratorAttributes attribs;
    attribs.config.overlapSupport = true;
    generator.setAttributes(attribs);
    generator.setThreadCount(THREAD_COUNT);
    generator.generate(m_Glyphs.data(), m_Glyphs.size());

    msdfgen::BitmapConstRef<float, 3> bitmap = generator.atlasStorage();
    uint8_t *data = (uint8_t *)bitmap.pixels;

    // Create atlas texture
    glGenTextures(1, &m_TextureHandle);
    glBindTexture(GL_TEXTURE_2D, m_TextureHandle);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);

    // Set texture options
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);

    msdfgen::destroyFont(font);
    msdfgen::deinitializeFreetype(ft);
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
    uint32_t indexCount = 0;
    std::array<Font *, 32> fonts;

    std::shared_ptr<VertexArray> vertexArray;
    std::shared_ptr<VertexBuffer> vertexBuffer;
    std::shared_ptr<IndexBuffer> indexBuffer;
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
    s_TextData.vertexBuffer->SetAttributes(
    {
        {VertexAttribType::VECTOR_FLOAT_3, false},
        {VertexAttribType::VECTOR_FLOAT_3, false},
        {VertexAttribType::VECTOR_FLOAT_2, false},
        {VertexAttribType::INT, false},
    }, sizeof(FontVertex));

    uint32_t* quadIndices = new uint32_t[s_TextData.MAX_INDICES];
	uint32_t offset = 0;
	for (uint32_t i = 0; i < s_TextData.MAX_INDICES; i += 6)
	{
		quadIndices[i + 0] = offset + 0;
		quadIndices[i + 1] = offset + 1;
		quadIndices[i + 2] = offset + 2;

		quadIndices[i + 3] = offset + 2;
		quadIndices[i + 4] = offset + 3;
		quadIndices[i + 5] = offset + 0;

		offset += 4;
	}
    
    s_TextData.indexBuffer = std::make_shared<IndexBuffer>(quadIndices, s_TextData.MAX_INDICES);
	delete[] quadIndices;

    s_TextData.vertexArray->SetVertexBuffer(s_TextData.vertexBuffer);
    s_TextData.vertexArray->SetIndexBuffer(s_TextData.indexBuffer);
}

void TextRenderer::Shutdown()
{
    if (s_TextData.vertexPointerBase)
        delete[] s_TextData.vertexPointerBase;
}

void TextRenderer::Begin(const glm::mat4 &viewProjection)
{
    s_TextData.vertexPointer = s_TextData.vertexPointerBase;
    s_TextData.indexCount = 0;

    s_TextData.shader->Use();
    s_TextData.shader->SetUniform("viewProjection", viewProjection);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void TextRenderer::End()
{
    if (s_TextData.indexCount)
    {
        s_TextData.vertexArray->Bind();

        uint64_t byteSize = (uint64_t)((uint8_t *)(s_TextData.vertexPointer) - (uint8_t *)(s_TextData.vertexPointerBase));
        s_TextData.vertexBuffer->SetData(s_TextData.vertexPointerBase, byteSize);

        for (uint32_t i = 0; i < s_TextData.fontTextureIndex; ++i)
        {
            if (s_TextData.fonts[i])
            {
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, s_TextData.fonts[i]->GetTextureHandle());
            }
        }

        glDrawElements(GL_TRIANGLES, s_TextData.indexCount, GL_UNSIGNED_BYTE, nullptr);
    }
}

void TextRenderer::DrawString(Font *font, const std::string &text, const glm::mat4 &transform, const glm::vec3 &color, const TextParameter &params)
{
    if (s_TextData.indexCount + 6 * text.size() > s_TextData.MAX_VERTICES)
    {
        return;
    }

    int textureIndex = GetFontTextureIndex(font);

    // load store new font
    if (textureIndex == -1)
    {
        if (s_TextData.fontTextureIndex >= s_TextData.MAX_FONTS)
        {
            return;
        }
        textureIndex = s_TextData.fontTextureIndex++;
        s_TextData.fonts[textureIndex] = font;
    }

    const auto &fontGeometry = font->GetGeometry();
	const auto &metrics = fontGeometry.getMetrics();

    double x = 0.0;
	double y = 0.0;
	double max_x = 0.0; // to track the maximum width
	double min_y = 0.0; // to track the minimum y position (since y decrease with line breaks)

	double fontScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
	const double spaceGlyphAdvance = fontGeometry.getGlyph(' ')->getAdvance();

	for (size_t i = 0; i < text.size(); i++)
	{
		char character = text[i];
		if (character == '\r')
		{
			continue;
		}

		if (character == '\n')
		{
			max_x = std::max(max_x, x);

			x = 0.0;
			y -= fontScale * metrics.lineHeight + params.lineSpacing;
			continue;
		}

		if (character == ' ')
		{
			float advance = (float)spaceGlyphAdvance;
			if (i < text.size() - 1)
			{
				char nextCharacter = text[i + 1];
				double dAdvance;
				fontGeometry.getAdvance(dAdvance, character, nextCharacter);
				advance = (float)advance;
			}

			x += fontScale * advance + params.kerning;
			continue;
		}

		if (character == '\t')
		{
			x += 4.0f * (fontScale * spaceGlyphAdvance + params.kerning);
			continue;
		}

		auto glyph = fontGeometry.getGlyph(character);
		if (!glyph)
		{
			glyph = fontGeometry.getGlyph('?');
		}

		double atlasLeft, atlasBottom, atlasRight, atlasTop;
		glyph->getQuadAtlasBounds(atlasLeft, atlasBottom, atlasRight, atlasTop);
		glm::vec2 texCoordMin((float)atlasLeft, (float)atlasBottom);
		glm::vec2 texCoordMax((float)atlasRight, (float)atlasTop);

		double planeLeft, planeBottom, planeRight, planeTop;
		glyph->getQuadPlaneBounds(planeLeft, planeBottom, planeRight, planeTop);
		glm::vec2 quadMin((float)planeLeft, (float)planeBottom);
		glm::vec2 quadMax((float)planeRight, (float)planeTop);

		quadMin *= fontScale;
		quadMax *= fontScale;

		quadMin += glm::vec2(x, y);
		quadMax += glm::vec2(x, y);

		float texel_width = 1.0f / font->GetAtlasSize().x;
		float texel_height = 1.0f / font->GetAtlasSize().y;

		texCoordMin *= glm::vec2(texel_width, texel_height);
		texCoordMax *= glm::vec2(texel_width, texel_height);

        s_TextData.vertexPointer->position = transform * glm::vec4(quadMin, 0.0f, 1.0f);
        s_TextData.vertexPointer->color = color;
        s_TextData.vertexPointer->uv = texCoordMin;
        s_TextData.vertexPointer->textureIndex = textureIndex;
        s_TextData.vertexPointer++;

        s_TextData.vertexPointer->position = transform * glm::vec4(quadMin.x, quadMax.y, 0.0f, 1.0f);
        s_TextData.vertexPointer->color = color;
        s_TextData.vertexPointer->uv = { texCoordMin.x, texCoordMax.y };
        s_TextData.vertexPointer->textureIndex = textureIndex;
        s_TextData.vertexPointer++;

        s_TextData.vertexPointer->position = transform * glm::vec4(quadMax, 0.0f, 1.0f);
        s_TextData.vertexPointer->color = color;
        s_TextData.vertexPointer->uv = texCoordMax;
        s_TextData.vertexPointer->textureIndex = textureIndex;
        s_TextData.vertexPointer++;

        s_TextData.vertexPointer->position = transform * glm::vec4(quadMax.x, quadMin.y, 0.0f, 1.0f);
        s_TextData.vertexPointer->color = color;
        s_TextData.vertexPointer->uv = { texCoordMax.x, texCoordMin.y };
        s_TextData.vertexPointer->textureIndex = textureIndex;
        s_TextData.vertexPointer++;

        s_TextData.indexCount += 6;

		if (i < text.size())
		{
			double advance = glyph->getAdvance();
			char nextCharacter = text[i + 1];
			fontGeometry.getAdvance(advance, character, nextCharacter);
			x += fontScale * advance + params.kerning;
		}

		max_x = std::max(max_x, x);
		min_y = std::min(min_y, y);
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
