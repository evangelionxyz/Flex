// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "Renderer2D.h"

#include "Core/Types.h"
#include "Renderer.h"
#include "Shader.h"
#include "VertexArray.h"
#include "VertexBuffer.h"

#include <glm/glm.hpp>

#include <array>
#include <vector>
#include <cassert>
#include <string_view>
#include <cstdint>

#include <glad/glad.h>

namespace flex
{
	namespace
	{
		struct LineVertex
		{
			glm::vec3 position;
			glm::vec4 color;
		};

		struct Renderer2DData
		{
			static constexpr uint32_t MaxLines = 10'000;
			static constexpr uint32_t MaxVertices = MaxLines * 2;

			Ref<VertexArray> vertexArray;
			Ref<VertexBuffer> vertexBuffer;
			Ref<Shader> shader;

			std::vector<LineVertex> vertexBufferData;
			uint32_t vertexCount = 0;
			glm::mat4 viewProjection = glm::mat4(1.0f);
			float lineWidth = 1.0f;
			bool initialized = false;
		};

		Renderer2DData s_Data;

		constexpr std::string_view kLineShaderName = "DebugLines";
	}

	void Renderer2D::Init()
	{
		if (s_Data.initialized)
		{
			return;
		}

		s_Data.vertexArray = CreateRef<VertexArray>();
		s_Data.vertexBuffer = CreateRef<VertexBuffer>(Renderer2DData::MaxVertices * sizeof(LineVertex));
		s_Data.vertexArray->Bind();
		s_Data.vertexBuffer->SetAttributes(
			{
				{ VertexAttribType::VECTOR_FLOAT_3, false },
				{ VertexAttribType::VECTOR_FLOAT_4, false }
			},
			sizeof(LineVertex));
		s_Data.vertexArray->SetVertexBuffer(s_Data.vertexBuffer);

		s_Data.vertexBufferData.resize(Renderer2DData::MaxVertices);

		s_Data.shader = Renderer::CreateShaderFromFile(
			{
				{ "Resources/shaders/line.vert.glsl", GL_VERTEX_SHADER },
				{ "Resources/shaders/line.frag.glsl", GL_FRAGMENT_SHADER }
			}, std::string(kLineShaderName));

		s_Data.vertexCount = 0;
		s_Data.lineWidth = 1.5f;
		s_Data.initialized = true;
	}

	void Renderer2D::Shutdown()
	{
		if (!s_Data.initialized)
		{
			return;
		}

		s_Data.vertexBufferData.clear();
		s_Data.vertexBufferData.shrink_to_fit();
		s_Data.vertexArray.reset();
		s_Data.vertexBuffer.reset();
		s_Data.shader.reset();
		s_Data.vertexCount = 0;
		s_Data.initialized = false;
	}

	void Renderer2D::BeginBatch(const glm::mat4& viewProjection)
	{
		assert(s_Data.initialized && "Renderer2D::Init must be called before BeginBatch");

		s_Data.viewProjection = viewProjection;
		s_Data.vertexCount = 0;
	}

	void Renderer2D::EndBatch()
	{
		Flush();
	}

	void Renderer2D::Flush()
	{
		if (!s_Data.initialized || s_Data.vertexCount == 0)
		{
			return;
		}

		const uint64_t dataSize = static_cast<uint64_t>(s_Data.vertexCount) * sizeof(LineVertex);
		s_Data.vertexBuffer->SetData(s_Data.vertexBufferData.data(), dataSize);

		s_Data.shader->Use();
		s_Data.shader->SetUniform("u_ViewProjection", s_Data.viewProjection);

		s_Data.vertexArray->Bind();
		glLineWidth(s_Data.lineWidth);
		glDrawArrays(GL_LINES, 0, s_Data.vertexCount);

		s_Data.vertexCount = 0;
	}

	void Renderer2D::DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color)
	{
		assert(s_Data.initialized && "Renderer2D::Init must be called before DrawLine");

		if (s_Data.vertexCount + 2 > Renderer2DData::MaxVertices)
		{
			Flush();
		}

		s_Data.vertexBufferData[s_Data.vertexCount++] = { p0, color };
		s_Data.vertexBufferData[s_Data.vertexCount++] = { p1, color };
	}

	void Renderer2D::SetLineWidth(float width)
	{
		s_Data.lineWidth = width;
	}

}