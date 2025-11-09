// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "Mesh.h"
#include "Material.h"
#include <iostream>
#include <filesystem>
#include <cassert>

#ifndef GLM_ENABLE_EXPERIMENTAL
    #define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace flex
{
    // Definition of the static mesh cache
    std::unordered_map<MeshKey, Ref<Mesh>, MeshKeyHasher, MeshKeyEqual> MeshLoader::m_MeshCache;

    Mesh::Mesh(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices)
    {
        this->vertexArray = CreateRef<VertexArray>();
        this->vertexBuffer = CreateRef<VertexBuffer>(vertices.data(), vertices.size() * sizeof(Vertex));
        this->indexBuffer = CreateRef<IndexBuffer>(indices.data(), static_cast<uint32_t>(indices.size()));

        vertexBuffer->SetAttributes(
            {
                {VertexAttribType::VECTOR_FLOAT_3, false}, // position
                {VertexAttribType::VECTOR_FLOAT_3, false}, // normal
                {VertexAttribType::VECTOR_FLOAT_3, false}, // tangent
                {VertexAttribType::VECTOR_FLOAT_3, false}, // bitangent
                {VertexAttribType::VECTOR_FLOAT_3, false}, // color
                {VertexAttribType::VECTOR_FLOAT_2, false}, // uv
            },
            sizeof(Vertex)
        );
        vertexArray->SetVertexBuffer(vertexBuffer);
        vertexArray->SetIndexBuffer(indexBuffer);
    }

    Ref<Mesh> Mesh::Create(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices)
    {
        return CreateRef<Mesh>(vertices, indices);
    }

    MeshInstance::MeshInstance(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices)
    {
        mesh = Mesh::Create(vertices, indices);
        material = CreateRef<Material>();
    }
    Ref<MeshInstance> MeshInstance::Create(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices)
    {
        return CreateRef<MeshInstance>(vertices, indices);
    }

    // Helper to build mat4 from glTF node TRS
    static glm::mat4 BuildNodeLocalMatrix(const tinygltf::Node &node)
    {
        glm::mat4 m(1.0f);
        if (!node.matrix.empty())
        {
            // glTF supplies 16 values column-major. Construct manually.
            m = glm::mat4(
                (float)node.matrix[0],  (float)node.matrix[1],  (float)node.matrix[2],  (float)node.matrix[3],
                (float)node.matrix[4],  (float)node.matrix[5],  (float)node.matrix[6],  (float)node.matrix[7],
                (float)node.matrix[8],  (float)node.matrix[9],  (float)node.matrix[10], (float)node.matrix[11],
                (float)node.matrix[12], (float)node.matrix[13], (float)node.matrix[14], (float)node.matrix[15]
            );
            return m;
        }
        glm::vec3 translation(0.0f);
        glm::quat rotation = glm::quat(1.0f,0.0f,0.0f,0.0f);
        glm::vec3 scale(1.0f);
        if (!node.translation.empty())
        {
            translation = glm::vec3(node.translation[0], node.translation[1], node.translation[2]);
        }
        if (!node.rotation.empty())
        {
            rotation = glm::quat((float)node.rotation[3], (float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2]);
        }
        if (!node.scale.empty())
        {
            scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
        }

        m = glm::translate(glm::mat4(1.0f), translation) * glm::toMat4(rotation) * glm::scale(glm::mat4(1.0f), scale);
        return m;
    }

    MeshScene MeshLoader::LoadSceneGraphFromGLTF(const std::string &filename)
    {
        MeshScene scene;
        tinygltf::Model gltfModel;
        tinygltf::TinyGLTF loader;
        std::string err, warn;

        bool ok = false;
        if (filename.ends_with(".glb"))
            ok = loader.LoadBinaryFromFile(&gltfModel, &err, &warn, filename);
        else
            ok = loader.LoadASCIIFromFile(&gltfModel, &err, &warn, filename);
        
        if (!ok)
            return scene;

        // Preload textures
        const auto textures = LoadTexturesFromGLTF(gltfModel);
        
        // Reserve nodes
        scene.nodes.resize(gltfModel.nodes.size());
        
        // Build raw node relationships and local transforms
        for (size_t i = 0;i < gltfModel.nodes.size();++i)
        {
            const tinygltf::Node &n = gltfModel.nodes[i];
            
            MeshNode &mn = scene.nodes[i];
            mn.name = n.name;
            mn.local = BuildNodeLocalMatrix(n);
            for (int c : n.children)
            {
                mn.children.push_back(c);
                scene.nodes[c].parent = static_cast<int>(i);
            }
        }

        // Identify roots
        for (size_t i = 0; i < scene.nodes.size(); ++i)
        {
            if (scene.nodes[i].parent < 0)
            {
                scene.roots.push_back(static_cast<int>(i));
            }
        } 

        // Load meshes referenced by nodes
        for (size_t i = 0; i < gltfModel.nodes.size(); ++i)
        {
            const tinygltf::Node &n = gltfModel.nodes[i];
            if (n.mesh < 0)
                continue;

            if (n.mesh >= (int)gltfModel.meshes.size())
                continue;

            const tinygltf::Mesh &gltfMesh = gltfModel.meshes[n.mesh];

            for (const auto &primitive : gltfMesh.primitives)
            {
                std::vector<Vertex> vertices;
                std::vector<uint32_t> indices;

                // Get vertices
                LoadVertexData(vertices, primitive, gltfModel);

                // Get indices
                LoadIndicesData(indices, primitive, gltfModel);

                // Try to reuse an existing mesh from the cache using the vertex/index counts as key
                MeshKey key{ static_cast<uint32_t>(vertices.size()), static_cast<uint32_t>(indices.size()) };
                Ref<Mesh> mesh;
                auto it = m_MeshCache.find(key);
                if (it != m_MeshCache.end())
                {
                    mesh = it->second;
                }
                else
                {
                    mesh = Mesh::Create(vertices, indices);
                    m_MeshCache.emplace(key, mesh);
                }

                // Create Mesh Instance
                Ref<MeshInstance> meshInstance = CreateRef<MeshInstance>();
                meshInstance->mesh = mesh;
                meshInstance->material = CreateRef<Material>();
                meshInstance->meshIndex = static_cast<int>(scene.flatMeshes.size());

                // Material
                LoadMaterial(meshInstance, primitive, gltfModel.materials, textures);

                scene.nodes[i].meshInstances.push_back(meshInstance);
                scene.flatMeshes.push_back(meshInstance);
            }
        }

        // Compute world transforms via DFS
        std::function<void(int, const glm::mat4 &)> recurse = [&](const int nodeIndex, const glm::mat4 &parentWorld)
        {
            MeshNode &n = scene.nodes[nodeIndex];
            n.world = parentWorld * n.local;
            for (const auto &m : n.meshInstances)
            {
                m->localTransform = n.local;
                m->worldTransform = n.world;
            }

            for (const int c : n.children)
                recurse(c, n.world);
        };

        for (const int root : scene.roots)
            recurse(root, glm::mat4(1.0f));

        return scene;
    }

    void MeshLoader::ClearCache()
    {
        m_MeshCache.clear();
    }

    Ref<MeshInstance> MeshLoader::CreateFallbackQuad()
    {
        std::cout << "Creating fallback quad mesh\n";
        
        std::vector<Vertex> vertices;
        vertices.push_back({ glm::vec3{-0.5f,-0.5f, 0.0f }, glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{1.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f}, glm::vec3{ 1.0f, 0.0f, 1.0f },  glm::vec2{ 0.0f, 0.0f } });
        vertices.push_back({ glm::vec3{-0.5f, 0.5f, 0.0f }, glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{1.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f}, glm::vec3{ 1.0f, 0.0f, 1.0f },  glm::vec2{ 0.0f, 1.0f } });
        vertices.push_back({ glm::vec3{ 0.5f, 0.5f, 0.0f }, glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{1.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f}, glm::vec3{ 1.0f, 0.0f, 1.0f },  glm::vec2{ 1.0f, 1.0f } });
        vertices.push_back({ glm::vec3{ 0.5f,-0.5f, 0.0f }, glm::vec3{0.0f, 0.0f, 1.0f}, glm::vec3{1.0f, 0.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f}, glm::vec3{ 1.0f, 0.0f, 1.0f },  glm::vec2{ 1.0f, 0.0f } });

        std::vector<uint32_t> indices
        {
            0, 1, 2,
            0, 2, 3
        };

        Ref<MeshInstance> fallbackMeshInstance = MeshInstance::Create(vertices, indices);
        
        // Assign a default texture to fallback mesh
        fallbackMeshInstance->material->baseColorTexture = Renderer::GetMagentaTexture();
        
        return fallbackMeshInstance;
    }

    std::vector<Ref<Texture2D>> MeshLoader::LoadTexturesFromGLTF(const tinygltf::Model& model)
    {
        std::vector<Ref<Texture2D>> gltfTextures;
        std::cout << "Loading " << model.textures.size() << " textures from glTF\n";
        
        for (size_t i = 0; i < model.textures.size(); ++i)
        {
            const tinygltf::Texture& gltfTexture = model.textures[i];
            
            if (gltfTexture.source >= 0 && gltfTexture.source < model.images.size())
            {
                const tinygltf::Image& image = model.images[gltfTexture.source];
                
                std::cout << "  Texture " << i << ": " << image.name << " (" << image.width << "x" << image.height << ")\n";
                
                // Create texture from image data
                TextureCreateInfo createInfo;
                createInfo.width = image.width;
                createInfo.height = image.height;
                createInfo.flip = true;
                createInfo.clampMode = WrapMode::REPEAT;
                createInfo.filter = FilterMode::LINEAR;
                createInfo.format = Format::RGBA8;

                Ref<Texture2D> texture;
                
                if (!image.image.empty())
                {
                    // Image data is embedded in the glTF
                    texture = CreateRef<Texture2D>(createInfo, (void*)image.image.data(), image.image.size());
                    std::cout << "    Loaded embedded texture\n";
                }
                else if (!image.uri.empty())
                {

                    // Image is referenced by URI (external file)
                    std::string texturePath = "Resources/models/" + image.uri;
                    
                    // Check if file exists
                    if (std::filesystem::exists(texturePath))
                    {
                        texture = CreateRef<Texture2D>(createInfo, texturePath);
                        std::cout << "    Loaded external texture: " << texturePath << "\n";
                    }
                }
                
                gltfTextures.push_back(texture);
            }
        }
        
        return gltfTextures;
    }

    const unsigned char* MeshLoader::GetBufferData(const tinygltf::Model& model, const tinygltf::Accessor& accessor)
    {
        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        return &buffer.data[bufferView.byteOffset + accessor.byteOffset];
    }

    Ref<MeshInstance> MeshLoader::CreateSkyboxCube()
    {
        std::cout << "Creating skybox cube mesh\n";
        
        // Skybox cube vertices (positions only)
        std::vector<Vertex> vertices;
        
        // Front face
        vertices.push_back({ glm::vec3(-1.0f, -1.0f,  1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(0.0f, 0.0f) });
        vertices.push_back({ glm::vec3( 1.0f, -1.0f,  1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(1.0f, 0.0f) });
        vertices.push_back({ glm::vec3( 1.0f,  1.0f,  1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(1.0f, 1.0f) });
        vertices.push_back({ glm::vec3(-1.0f,  1.0f,  1.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(0.0f, 1.0f) });
        
        // Back face
        vertices.push_back({ glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(1.0f, 0.0f) });
        vertices.push_back({ glm::vec3(-1.0f,  1.0f, -1.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(1.0f, 1.0f) });
        vertices.push_back({ glm::vec3( 1.0f,  1.0f, -1.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(0.0f, 1.0f) });
        vertices.push_back({ glm::vec3( 1.0f, -1.0f, -1.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(0.0f, 0.0f) });
        
        // Left face
        vertices.push_back({ glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(0.0f, 0.0f) });
        vertices.push_back({ glm::vec3(-1.0f, -1.0f,  1.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(1.0f, 0.0f) });
        vertices.push_back({ glm::vec3(-1.0f,  1.0f,  1.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(1.0f, 1.0f) });
        vertices.push_back({ glm::vec3(-1.0f,  1.0f, -1.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(0.0f, 1.0f) });
        
        // Right face
        vertices.push_back({ glm::vec3( 1.0f, -1.0f, -1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(1.0f, 0.0f) });
        vertices.push_back({ glm::vec3( 1.0f,  1.0f, -1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(1.0f, 1.0f) });
        vertices.push_back({ glm::vec3( 1.0f,  1.0f,  1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(0.0f, 1.0f) });
        vertices.push_back({ glm::vec3( 1.0f, -1.0f,  1.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f), glm::vec2(0.0f, 0.0f) });
        
        // Bottom face
        vertices.push_back({ glm::vec3(-1.0f, -1.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f), glm::vec2(0.0f, 1.0f) });
        vertices.push_back({ glm::vec3( 1.0f, -1.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f), glm::vec2(1.0f, 1.0f) });
        vertices.push_back({ glm::vec3( 1.0f, -1.0f,  1.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f), glm::vec2(1.0f, 0.0f) });
        vertices.push_back({ glm::vec3(-1.0f, -1.0f,  1.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f), glm::vec2(0.0f, 0.0f) });
        
        // Top face
        vertices.push_back({ glm::vec3(-1.0f,  1.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f), glm::vec2(0.0f, 0.0f) });
        vertices.push_back({ glm::vec3(-1.0f,  1.0f,  1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f), glm::vec2(0.0f, 1.0f) });
        vertices.push_back({ glm::vec3( 1.0f,  1.0f,  1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f), glm::vec2(1.0f, 1.0f) });
        vertices.push_back({ glm::vec3( 1.0f,  1.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(1.0f), glm::vec2(1.0f, 0.0f) });

        std::vector<uint32_t> indices = {
            // Front face
            0, 1, 2,  2, 3, 0,
            // Back face
            4, 5, 6,  6, 7, 4,
            // Left face
            8, 9, 10,  10, 11, 8,
            // Right face
            12, 13, 14,  14, 15, 12,
            // Bottom face
            16, 17, 18,  18, 19, 16,
            // Top face
            20, 21, 22,  22, 23, 20
        };

        Ref<MeshInstance> skyboxMeshInstance = MeshInstance::Create(vertices, indices);

        return skyboxMeshInstance;
    }

    void MeshLoader::LoadMaterial(const Ref<MeshInstance>& meshInstance, const tinygltf::Primitive &primitive, const std::vector<tinygltf::Material> &materials, const std::vector<Ref<Texture2D>> &loadedTextures)
    {
        if (!meshInstance->material)
        {
            meshInstance->material = CreateRef<Material>();
        }

        // Assign texture based on material
        meshInstance->materialIndex = primitive.material;
        if (primitive.material >= 0 && primitive.material < materials.size())
        {
            const tinygltf::Material& material = materials[primitive.material];
            std::cout << "  Material: " << material.name << "\n";

            meshInstance->material->name = material.name;
            meshInstance->material->params.baseColorFactor = {material.pbrMetallicRoughness.baseColorFactor[0], material.pbrMetallicRoughness.baseColorFactor[1], material.pbrMetallicRoughness.baseColorFactor[2], 1.0f };
            meshInstance->material->params.emissiveFactor = {material.emissiveFactor[0], material.emissiveFactor[1], material.emissiveFactor[2], 1.0f };
            meshInstance->material->params.metallicFactor = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);
            meshInstance->material->params.roughnessFactor = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);
            meshInstance->material->params.occlusionStrength = static_cast<float>(material.occlusionTexture.strength);
            
            // base color texture
            const int baseColorIndex = material.pbrMetallicRoughness.baseColorTexture.index;
            if (baseColorIndex >= 0 && baseColorIndex < loadedTextures.size())
            {
                meshInstance->material->baseColorTexture = loadedTextures[baseColorIndex];
            }

            // emissive texture
            const int emissiveIndex = material.emissiveTexture.index;
            if (emissiveIndex >= 0 && emissiveIndex < loadedTextures.size())
            {
                meshInstance->material->emissiveTexture = loadedTextures[emissiveIndex];
            }

            // metallic roughness texture
            const int metallicRoughnessIndex = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
            if (metallicRoughnessIndex >= 0 && metallicRoughnessIndex < loadedTextures.size())
            {
                meshInstance->material->metallicRoughnessTexture = loadedTextures[metallicRoughnessIndex];
            }

            // normal texture
            const int normalIndex = material.normalTexture.index;
            if (normalIndex >= 0 && normalIndex < loadedTextures.size())
            {
                meshInstance->material->normalTexture = loadedTextures[normalIndex];
            }

            // occlusion texture
            const int occlusionIndex = material.occlusionTexture.index;
            if (occlusionIndex >= 0 && occlusionIndex < loadedTextures.size())
            {
                meshInstance->material->occlusionTexture = loadedTextures[occlusionIndex];
            }
        }
    }

    void MeshLoader::LoadVertexData(std::vector<Vertex> &vertices, const tinygltf::Primitive &primitive, const tinygltf::Model &model)
    {
        // Get vertex positions
        glm::vec3* positions = nullptr;
        size_t positionCount = 0;

        if (primitive.attributes.contains("POSITION"))
        {
            const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("POSITION")];
            positions = (glm::vec3*)GetBufferData(model, accessor);
            positionCount = accessor.count;
            std::cout << "  Found " << positionCount << " positions\n";
        }

        // Get vertex normals (optional)
        glm::vec3* normals = nullptr;
        if (primitive.attributes.contains("NORMAL"))
        {
            const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("NORMAL")];
            normals = (glm::vec3 *)GetBufferData(model, accessor);
            std::cout << "  Found normals\n";
        }

        // Get vertex tangents (optional)
        glm::vec4* tangents = nullptr;
        if (primitive.attributes.contains("TANGENT"))
        {
            const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TANGENT")];
            tangents = (glm::vec4 *)GetBufferData(model, accessor);
            std::cout << "  Found tangents\n";
        }

        // Get texture coordinates (optional)
        glm::vec2* texCoords = nullptr;
        if (primitive.attributes.contains("TEXCOORD_0"))
        {
            const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
            texCoords = (glm::vec2 *)GetBufferData(model, accessor);
            std::cout << "  Found texture coordinates\n";
        }

        // Build vertices
        vertices.reserve(positionCount);
        for (size_t i = 0; i < positionCount; ++i)
        {
            Vertex vertex{};
            vertex.position = positions[i];
            vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);

            if (normals)
            {
                vertex.normal = normals[i];
            }

            if (tangents)
            {
                vertex.tangent = glm::vec3(tangents[i]);
                vertex.bitangent = glm::cross(vertex.normal, vertex.tangent) * tangents[i].w;
            }
            else if (normals)
            {
                // Generate tangent space if not provided
                // Simple approach: assume UV-aligned tangent
                vertex.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
                vertex.bitangent = glm::cross(vertex.normal, vertex.tangent);
            }

            if (texCoords)
            {
                vertex.uv = texCoords[i];
            }

            vertices.push_back(vertex);
        }
    }

    void MeshLoader::LoadIndicesData(std::vector<uint32_t> &indices, const tinygltf::Primitive &primitive, const tinygltf::Model &model)
    {
        if (primitive.indices >= 0)
        {
            const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
            const unsigned char* indexData = GetBufferData(model, indexAccessor);

            std::cout << "  Found " << indexAccessor.count << " indices\n";

            // Handle different index types
            if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            {
                const auto indexPtr = reinterpret_cast<const uint16_t *>(indexData);
                for (size_t i = 0; i < indexAccessor.count; ++i)
                {
                    indices.push_back(indexPtr[i]);
                }
            }
            else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
            {
                const auto indexPtr = reinterpret_cast<const uint32_t *>(indexData);
                for (size_t i = 0; i < indexAccessor.count; ++i)
                {
                    indices.push_back(indexPtr[i]);
                }
            }
            else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
            {
                const auto indexPtr = indexData;
                for (size_t i = 0; i < indexAccessor.count; ++i)
                {
                    indices.push_back(indexPtr[i]);
                }
            }
        }
    }
}