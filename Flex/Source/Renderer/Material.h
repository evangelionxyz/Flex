// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef MATERIAL_H
#define MATERIAL_H

#include "Texture.h"
#include "Renderer.h"
#include <glm/glm.hpp>

namespace flex
{
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

        std::shared_ptr<Texture2D> baseColorTexture;
        std::shared_ptr<Texture2D> emissiveTexture;
        std::shared_ptr<Texture2D> metallicRoughnessTexture;
        std::shared_ptr<Texture2D> normalTexture;
        std::shared_ptr<Texture2D> occlusionTexture;

        MaterialType type = MaterialType::Opaque;
    };
}

#endif