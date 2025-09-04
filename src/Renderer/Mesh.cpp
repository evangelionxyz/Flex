#include "Mesh.h"
#include <iostream>
#include <filesystem>
#include <cassert>

Mesh::Mesh(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices)
{
    this->vertexArray = std::make_shared<VertexArray>();
    this->vertexBuffer = std::make_shared<VertexBuffer>(vertices.data(), vertices.size() * sizeof(Vertex));
    this->indexBuffer = std::make_shared<IndexBuffer>(indices.data(), indices.size() * sizeof(uint32_t), static_cast<uint32_t>(indices.size()));
    this->material = std::make_shared<Material>();
}

std::shared_ptr<Mesh> Mesh::Create(const std::vector<Vertex> &vertices, const std::vector<uint32_t> &indices)
{
    return std::make_shared<Mesh>(vertices, indices);
}

std::vector<std::shared_ptr<Mesh>> MeshLoader::LoadFromGLTF(const std::string& filePath)
{
    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF tgltfLoader;
    std::string errMsg, warnMsg;
    
    bool ret = false;
    if (filePath.ends_with(".glb"))
    {
        ret = tgltfLoader.LoadBinaryFromFile(&gltfModel, &errMsg, &warnMsg, filePath);
    }
    else
    {
        ret = tgltfLoader.LoadASCIIFromFile(&gltfModel, &errMsg, &warnMsg, filePath);
    }
    
    if (!errMsg.empty())
    {
        std::cout << "glTF Error: " << errMsg << "\n";
    }

    if (!warnMsg.empty())
    {
        std::cout << "glTF Warning: " << warnMsg << "\n";
    }

    if (!ret)
    {
        std::cerr << "Failed to parse glTF: " << filePath << "\n";
        return {};
    }

    std::vector<std::shared_ptr<Texture2D>> gltfTextures = LoadTexturesFromGLTF(gltfModel);
    std::vector<std::shared_ptr<Mesh>> meshes;

    for (const auto& gltfMesh : gltfModel.meshes)
    {
        std::cout << "Processing mesh: " << gltfMesh.name << "\n";
        
        for (const auto& primitive : gltfMesh.primitives)
        {
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            // Get vertex positions
            glm::vec3* positions = nullptr;
            size_t positionCount = 0;
            if (primitive.attributes.find("POSITION") != primitive.attributes.end())
            {
                const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.attributes.at("POSITION")];
                positions = (glm::vec3*)GetBufferData(gltfModel, accessor);
                positionCount = accessor.count;
                std::cout << "  Found " << positionCount << " positions\n";
            }

            // Get vertex normals (optional)
            glm::vec3* normals = nullptr;
            if (primitive.attributes.find("NORMAL") != primitive.attributes.end())
            {
                const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.attributes.at("NORMAL")];
                normals = (glm::vec3*)GetBufferData(gltfModel, accessor);
                std::cout << "  Found normals\n";
            }

            // Get vertex tangents (optional)
            glm::vec4* tangents = nullptr;
            if (primitive.attributes.find("TANGENT") != primitive.attributes.end())
            {
                const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.attributes.at("TANGENT")];
                tangents = (glm::vec4*)GetBufferData(gltfModel, accessor);
                std::cout << "  Found tangents\n";
            }

            // Get texture coordinates (optional)
            glm::vec2* texCoords = nullptr;
            if (primitive.attributes.find("TEXCOORD_0") != primitive.attributes.end())
            {
                const tinygltf::Accessor& accessor = gltfModel.accessors[primitive.attributes.at("TEXCOORD_0")];
                texCoords = (glm::vec2*)GetBufferData(gltfModel, accessor);
                std::cout << "  Found texture coordinates\n";
            }

            // Build vertices
            vertices.reserve(positionCount);
            for (size_t i = 0; i < positionCount; ++i)
            {
                Vertex vertex;
                
                vertex.position = positions[i];
                vertex.color = glm::vec3(1.0f, 1.0f, 1.0f);
                
                if (normals)
                    vertex.normal = normals[i];
                
                if (tangents)
                {
                    vertex.tangent = glm::vec3(tangents[i]);
                    // Calculate bitangent from normal and tangent
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
                    vertex.uv = texCoords[i];
                
                vertices.push_back(vertex);
            }

            // Get indices
            if (primitive.indices >= 0)
            {
                const tinygltf::Accessor& indexAccessor = gltfModel.accessors[primitive.indices];
                const unsigned char* indexData = GetBufferData(gltfModel, indexAccessor);
                
                std::cout << "  Found " << indexAccessor.count << " indices\n";
                
                // Handle different index types
                if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                {
                    const uint16_t* indexPtr = (const uint16_t*)indexData;
                    for (size_t i = 0; i < indexAccessor.count; ++i)
                    {
                        indices.push_back(static_cast<uint32_t>(indexPtr[i]));
                    }
                }
                else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
                {
                    const uint32_t* indexPtr = (const uint32_t*)indexData;
                    for (size_t i = 0; i < indexAccessor.count; ++i)
                    {
                        indices.push_back(indexPtr[i]);
                    }
                }
                else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                {
                    const uint8_t* indexPtr = (const uint8_t*)indexData;
                    for (size_t i = 0; i < indexAccessor.count; ++i)
                    {
                        indices.push_back(static_cast<uint32_t>(indexPtr[i]));
                    }
                }
            }
            else
            {
                // No indices, create them sequentially
                for (size_t i = 0; i < vertices.size(); ++i)
                {
                    indices.push_back(static_cast<uint32_t>(i));
                }
            }

            // Create OpenGL objects for this primitive
            std::shared_ptr<Mesh> mesh = Mesh::Create(vertices, indices);
            mesh->vertexBuffer->SetAttributes(
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
            
            mesh->vertexArray->SetVertexBuffer(mesh->vertexBuffer);
            mesh->vertexArray->SetIndexBuffer(mesh->indexBuffer);
            
            // Assign texture based on material
            mesh->materialIndex = primitive.material;
            if (primitive.material >= 0 && primitive.material < gltfModel.materials.size())
            {
                const tinygltf::Material& material = gltfModel.materials[primitive.material];
                std::cout << "  Material: " << material.name << "\n";

                mesh->material->baseColorFactor = {material.pbrMetallicRoughness.baseColorFactor[0], material.pbrMetallicRoughness.baseColorFactor[1], material.pbrMetallicRoughness.baseColorFactor[2] };
                mesh->material->emissiveFactor = {material.emissiveFactor[0], material.emissiveFactor[1], material.emissiveFactor[2] };
                mesh->material->metallicFactor = material.pbrMetallicRoughness.metallicFactor;
                mesh->material->roughnessFactor = material.pbrMetallicRoughness.roughnessFactor;
                mesh->material->occlusionStrength = material.occlusionTexture.strength;
                
                // base color texture
                const int baseColorIndex = material.pbrMetallicRoughness.baseColorTexture.index;
                if (baseColorIndex >= 0 && baseColorIndex < gltfTextures.size())
                {
                    mesh->material->baseColorTexture = gltfTextures[baseColorIndex];
                }

                // emissive texture
                const int emissiveIndex = material.emissiveTexture.index;
                if (emissiveIndex >= 0 && emissiveIndex < gltfTextures.size())
                {
                    mesh->material->emissiveTexture = gltfTextures[emissiveIndex];
                }

                // metallic roughness texture
                const int metallicRoughnessIndex = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
                if (metallicRoughnessIndex >= 0 && metallicRoughnessIndex < gltfTextures.size())
                {
                    mesh->material->metallicRoughnessTexture = gltfTextures[metallicRoughnessIndex];
                }

                // normal texture
                const int normalIndex = material.normalTexture.index;
                if (normalIndex >= 0 && normalIndex < gltfTextures.size())
                {
                    mesh->material->normalTexture = gltfTextures[normalIndex];
                }

                // occlusion texture
                const int occlusionIndex = material.occlusionTexture.index;
                if (occlusionIndex >= 0 && occlusionIndex < gltfTextures.size())
                {
                    mesh->material->occlusionTexture = gltfTextures[occlusionIndex];
                }
            }
            
            meshes.push_back(mesh);
            std::cout << "  Created mesh with " << vertices.size() << " vertices and " << indices.size() << " indices\n";
        }
    }

    std::cout << "Loaded " << meshes.size() << " mesh(es) from glTF file\n";
    return meshes;
}

std::shared_ptr<Mesh> MeshLoader::CreateFallbackQuad()
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

    std::shared_ptr<Mesh> fallbackMesh = Mesh::Create(vertices, indices);
    fallbackMesh->vertexBuffer->SetAttributes(
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
    fallbackMesh->vertexArray->SetVertexBuffer(fallbackMesh->vertexBuffer);
    fallbackMesh->vertexArray->SetIndexBuffer(fallbackMesh->indexBuffer);
    
    // Assign a default texture to fallback mesh
    fallbackMesh->material->baseColorTexture = Renderer::GetMagentaTexture();
    
    return fallbackMesh;
}

std::vector<std::shared_ptr<Texture2D>> MeshLoader::LoadTexturesFromGLTF(const tinygltf::Model& model)
{
    std::vector<std::shared_ptr<Texture2D>> gltfTextures;
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
            createInfo.format = TextureFormat::RGBA8;
            createInfo.width = image.width;
            createInfo.height = image.height;

            std::shared_ptr<Texture2D> texture;
            
            if (!image.image.empty())
            {
                // Image data is embedded in the glTF
                texture = std::make_shared<Texture2D>(createInfo, (void*)image.image.data(), image.image.size());
                std::cout << "    Loaded embedded texture\n";
            }
            else if (!image.uri.empty())
            {

                // Image is referenced by URI (external file)
                std::string texturePath = "resources/models/" + image.uri;
                
                // Check if file exists
                if (std::filesystem::exists(texturePath))
                {
                    texture = std::make_shared<Texture2D>(createInfo, texturePath);
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

std::shared_ptr<Mesh> MeshLoader::CreateSkyboxCube()
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

    std::shared_ptr<Mesh> skyboxMesh = Mesh::Create(vertices, indices);
    skyboxMesh->vertexBuffer->SetAttributes(
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
    skyboxMesh->vertexArray->SetVertexBuffer(skyboxMesh->vertexBuffer);
    skyboxMesh->vertexArray->SetIndexBuffer(skyboxMesh->indexBuffer);
    
    return skyboxMesh;
}
