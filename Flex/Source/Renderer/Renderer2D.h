// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef RENDERER_2D_H
#define RENDERER_2D_H

#include "Math/Math.hpp"

namespace flex
{
    class Renderer2D
    {
    public:
        static void Init();
        static void Shutdown();

        static void BeginBatch(const glm::mat4& viewProjection);
        static void EndBatch();
        static void Flush();

        static void DrawLine(const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color);
        static void SetLineWidth(float width);
    };
}

#endif