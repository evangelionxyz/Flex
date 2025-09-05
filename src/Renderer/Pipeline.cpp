#include "Pipeline.h"

Pipeline::Pipeline(const PipelineCreateInfo &createInfo)
    : m_Shader(nullptr)
{
}

Pipeline::~Pipeline()
{
    m_Shader = nullptr;
}

void Pipeline::AddShader(Shader *shader)
{
    m_Shader = shader;
}
