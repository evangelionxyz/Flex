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

#define MAX_BONES 100
#define NUM_BONE_INFLUENCE 4

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

    Material()
    {
        // Neutral defaults per glTF PBR spec when a texture is absent
        baseColorTexture = Renderer::GetWhiteTexture();           // baseColorFactor will tint
        emissiveTexture = Renderer::GetWhiteTexture();            // no emissive
        metallicRoughnessTexture = Renderer::GetBlackTexture();   // will be overridden if texture present; factors supply values
        normalTexture = Renderer::GetWhiteTexture();              // flat normal
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

    // Local (node) transform and resolved world transform
    glm::mat4 localTransform {1.0f};
    glm::mat4 worldTransform {1.0f};

    Mesh(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);
    static std::shared_ptr<Mesh> Create(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);
};

// Scene graph structures
struct MeshNode
{
    int parent = -1;
    std::string name;
    std::vector<int> children;
    glm::mat4 local {1.0f};
    glm::mat4 world {1.0f};
    std::vector<std::shared_ptr<Mesh>> meshes; // Mesh primitives referenced by this node
};

struct MeshScene
{
    std::vector<MeshNode> nodes;      // All nodes
    std::vector<int> roots;           // Root node indices
    std::vector<std::shared_ptr<Mesh>> flatMeshes; // All meshes collected (for convenience)
};

class MeshLoader
{
public:
    static std::shared_ptr<Mesh> CreateFallbackQuad();
    static std::shared_ptr<Mesh> CreateSkyboxCube();

    static void LoadMaterial(const std::shared_ptr<Mesh>& mesh, const tinygltf::Primitive &primitive, const std::vector<tinygltf::Material> &materials, const std::vector<std::shared_ptr<Texture2D>> &loadedTextures);
    static void LoadVertexData(std::vector<Vertex> &vertices, const tinygltf::Primitive &primitive, const tinygltf::Model &model);
    static void LoadIndicesData(std::vector<uint32_t> &indices, const tinygltf::Primitive &primitive, const tinygltf::Model &model);

    // Load full scene graph retaining hierarchy & transforms
    static MeshScene LoadSceneGraphFromGLTF(const std::string &filename);

private:
    static std::vector<std::shared_ptr<Texture2D>> LoadTexturesFromGLTF(const tinygltf::Model& model);
    static const unsigned char* GetBufferData(const tinygltf::Model& model, const tinygltf::Accessor& accessor);
};
