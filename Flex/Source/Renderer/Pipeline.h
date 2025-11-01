// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef PIPELINE_H
#define PIPELINE_H

#include "RendererCommon.h"
#include "Shader.h"

namespace flex
{
    struct PipelineCreateInfo
    {
        RasterizeFillMode fillMode = RasterizeFillMode::SOLID;
        CullMode cullMode = CullMode::FRONT;
        ComparisonFunc comparisonFunc = ComparisonFunc::LESS_OR_EQUAL;

        bool enableDepth = true;
        bool enableBlend = true;
        bool enableDepthStencil = false;
    };

    class Pipeline
    {
    public:
        Pipeline(const PipelineCreateInfo &createInfo);
        ~Pipeline();

        void AddShader(Shader *shader);
        Shader *GetShader() { return m_Shader; }

    private:
        Shader *m_Shader;
    };
}

#endif