// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef MATERIAL_H
#define MATERIAL_H

#include "Texture.h"
#include "Renderer.h"
#include <glm/glm.hpp>

namespace flex
{
    class UniformBuffer;

    enum class MaterialType
    {
        Opaque,
        Transparent,
    };

    struct Material
    {
        Material();

        std::string name;

        struct Params
        {
            glm::vec4 baseColorFactor = glm::vec4(1.0f);
            glm::vec4 emissiveFactor = glm::vec4(0.0f);
            float metallicFactor = 1.0f;
            float roughnessFactor = 1.0f;
            float occlusionStrength = 0.0f;
        } params;

        Ref<Texture2D> baseColorTexture;
        Ref<Texture2D> emissiveTexture;
        Ref<Texture2D> metallicRoughnessTexture;
        Ref<Texture2D> normalTexture;
        Ref<Texture2D> occlusionTexture;

        Ref<Shader> shader;

        MaterialType type = MaterialType::Opaque;

        void UpdateData();
    private:
        Ref<UniformBuffer> m_Buffer;
    };
}

#endif