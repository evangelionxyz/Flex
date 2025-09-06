#pragma once

#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <tinygltf.h>

#include "VertexArray.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Texture.h"

#include "Renderer.h"

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 bitangent;
    glm::vec3 color;
    glm::vec2 uv;
};

struct Material
{
    glm::vec3 baseColorFactor = glm::vec3(1.0f);
    glm::vec3 emissiveFactor = glm::vec3(0.0f);
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float occlusionStrength = 1.0f;

    std::shared_ptr<Texture2D> baseColorTexture;
    std::shared_ptr<Texture2D> emissiveTexture;
    std::shared_ptr<Texture2D> metallicRoughnessTexture;
    std::shared_ptr<Texture2D> normalTexture;
    std::shared_ptr<Texture2D> occlusionTexture;

    Material()
    {
    // Neutral defaults per glTF PBR spec when a texture is absent
    baseColorTexture = Renderer::GetWhiteTexture();           // baseColorFactor will tint
    emissiveTexture = Renderer::GetBlackTexture();            // no emissive
    metallicRoughnessTexture = Renderer::GetBlackTexture();   // will be overridden if texture present; factors supply values
    normalTexture = Renderer::GetFlatNormalTexture();         // flat normal
    occlusionTexture = Renderer::GetWhiteTexture();           // full occlusion (no darkening)
    }
};

struct Mesh
{
    std::shared_ptr<VertexArray> vertexArray;
    std::shared_ptr<VertexBuffer> vertexBuffer;
    std::shared_ptr<IndexBuffer> indexBuffer;
    std::shared_ptr<Material> material;

    int materialIndex = -1;

    Mesh(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);
    static std::shared_ptr<Mesh> Create(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);
};

class MeshLoader
{
public:
    static std::vector<std::shared_ptr<Mesh>> LoadFromGLTF(const std::string& filename);
    static std::shared_ptr<Mesh> CreateFallbackQuad();
    static std::shared_ptr<Mesh> CreateSkyboxCube();

private:
    static std::vector<std::shared_ptr<Texture2D>> LoadTexturesFromGLTF(const tinygltf::Model& model);
    static const unsigned char* GetBufferData(const tinygltf::Model& model, const tinygltf::Accessor& accessor);
};
