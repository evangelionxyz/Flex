// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "Material.h"

#include "Renderer.h"
#include "UniformBuffer.h"
#include "Shader.h"

namespace flex
{
    Material::Material()
    {
        // Neutral defaults per glTF PBR spec when a texture is absent
        baseColorTexture = Renderer::GetWhiteTexture();           // baseColorFactor will tint
        emissiveTexture = Renderer::GetWhiteTexture();            // no emissive
        metallicRoughnessTexture = Renderer::GetBlackTexture();   // will be overridden if texture present; factors supply values
        normalTexture = Renderer::GetWhiteTexture();              // flat normal
        occlusionTexture = Renderer::GetWhiteTexture();           // full occlusion (no darkening)

        shader = Renderer::CreateShaderFromFile(
            {
                ShaderData{"resources/shaders/pbr.vert.glsl", GL_VERTEX_SHADER, 0},
                ShaderData{"resources/shaders/pbr.frag.glsl", GL_FRAGMENT_SHADER, 0}
            }, "MaterialPBR");

        m_Buffer = UniformBuffer::Create(sizeof(Material::Params), UNIFORM_BINDING_LOC_MATERIAL);
    }

    void Material::UpdateData()
    {
        m_Buffer->SetData(&params, sizeof(Material::Params));
    }
}