// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef MESH_H
#define MESH_H

#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <unordered_map>
#include <tinygltf.h>

#include "Core/Types.h"

#include "VertexArray.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "Texture.h"

#include "Renderer.h"

#define MAX_BONES 100
#define NUM_BONE_INFLUENCE 4

namespace flex
{
    struct Material;

    struct Vertex
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 tangent;
        glm::vec3 bitangent;
        glm::vec3 color;
        glm::vec2 uv;
    };

    // Mesh Primitives struct
    struct Mesh
    {
        Ref<VertexArray> vertexArray;
        Ref<VertexBuffer> vertexBuffer;
        Ref<IndexBuffer> indexBuffer;
        
        Mesh(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);
        static Ref<Mesh> Create(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);
    };

    // Actual Mesh Instance contains additional mesh data
    struct MeshInstance
    {
        Ref<Mesh> mesh;
        Ref<Material> material;
        int materialIndex = -1;
        int meshIndex = -1;

        // Local (node) transform and resolved world transform
        glm::mat4 localTransform {1.0f};
        glm::mat4 worldTransform {1.0f};

        MeshInstance() = default;
        MeshInstance(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);
        static Ref<MeshInstance> Create(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices);
    };

    // Scene graph structures
    struct MeshNode
    {
        int parent = -1;
        std::string name;
        std::vector<int> children;
        glm::mat4 local {1.0f};
        glm::mat4 world {1.0f};
        std::vector<Ref<MeshInstance>> meshInstances;
    };

    struct MeshScene
    {
        std::vector<MeshNode> nodes;      // All nodes
        std::vector<int> roots;           // Root node indices
        std::vector<Ref<MeshInstance>> flatMeshes; // All meshes collected (for convenience)
    };

    // Custom key for caching meshes. Two meshes are considered equal
    // if they have the same vertex and index counts.
    struct MeshKey
    {
        uint32_t vertexCount;
        uint32_t indicesCount;
    };

    struct MeshKeyHasher
    {
        std::size_t operator()(const MeshKey& k) const noexcept
        {
            uint64_t key = (static_cast<uint64_t>(k.vertexCount) << 32) ^ static_cast<uint64_t>(k.indicesCount);
            return std::hash<uint64_t>{}(key);
        }
    };

    struct MeshKeyEqual
    {
        bool operator()(const MeshKey& a, const MeshKey& b) const noexcept { return a.vertexCount == b.vertexCount && a.indicesCount == b.indicesCount; }
    };

    using MeshMap = std::unordered_map<MeshKey, Ref<Mesh>, MeshKeyHasher, MeshKeyEqual>;

    class MeshLoader
    {
    public:
        static Ref<MeshInstance> CreateFallbackQuad();
        static Ref<MeshInstance> CreateSkyboxCube();

        static void LoadMaterial(const Ref<MeshInstance>& meshInstance, const tinygltf::Primitive &primitive, const std::vector<tinygltf::Material> &materials, const std::vector<Ref<Texture2D>> &loadedTextures);
        static void LoadVertexData(std::vector<Vertex> &vertices, const tinygltf::Primitive &primitive, const tinygltf::Model &model);
        static void LoadIndicesData(std::vector<uint32_t> &indices, const tinygltf::Primitive &primitive, const tinygltf::Model &model);

        // Load full scene graph retaining hierarchy & transforms
        static MeshScene LoadSceneGraphFromGLTF(const std::string &filename);

    private:
        static std::vector<Ref<Texture2D>> LoadTexturesFromGLTF(const tinygltf::Model& model);
        static const unsigned char* GetBufferData(const tinygltf::Model& model, const tinygltf::Accessor& accessor);

        static MeshMap m_MeshCache;
    };
}

#endif