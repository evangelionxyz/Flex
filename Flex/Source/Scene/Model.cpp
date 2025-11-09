// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "Model.h"

#include "Renderer/Material.h"

namespace flex
{
    Model::Model(const std::string &filename)
        : m_Transform(glm::mat4(1.0f))
        , m_Scene(MeshLoader::LoadSceneGraphFromGLTF(filename))
    {
        m_MaterialUbo = UniformBuffer::Create(sizeof(Material::Params), UNIFORM_BINDING_LOC_MATERIAL);
    }

    Model::~Model()
    {
    }

    void Model::Update(float deltaTime)
    {
        for (MeshNode &node : m_Scene.nodes)
        {
            for (const Ref<MeshInstance> &meshInstnace : node.meshInstances)
            {
                
            }
        }
    }

    void Model::Render(Ref<Shader> &shader, const Ref<Texture2D> &environmentTexture)
    {
        for (MeshNode &node : m_Scene.nodes)
        {
            for (const Ref<MeshInstance> &meshInstance : node.meshInstances)
            {
                if (meshInstance->material)
                {
                    // Apply material parameters
                    m_MaterialUbo->SetData(&meshInstance->material->params, sizeof(Material::Params));

                    meshInstance->material->occlusionTexture->Bind(4);
					shader->SetUniform("u_OcclusionTexture", 4);
        
                    meshInstance->material->normalTexture->Bind(3);
                    shader->SetUniform("u_NormalTexture", 3);
        
                    meshInstance->material->metallicRoughnessTexture->Bind(2);
                    shader->SetUniform("u_MetallicRoughnessTexture", 2);
        
                    meshInstance->material->emissiveTexture->Bind(1);
                    shader->SetUniform("u_EmissiveTexture", 1);
        
                    meshInstance->material->baseColorTexture->Bind(0);
                    shader->SetUniform("u_BaseColorTexture", 0);
                }
        
                // Bind environment last to guarantee it stays on unit 5
                environmentTexture->Bind(5);
                shader->SetUniform("u_EnvironmentTexture", 5);
                
                // TODO: Calculate with mesh transform
                shader->SetUniform("u_Transform", m_Transform * meshInstance->localTransform);
        
                meshInstance->mesh->vertexArray->Bind();
                Renderer::DrawIndexed(meshInstance->mesh->vertexArray);
            }
        }
    }

    void Model::RenderDepth(Ref<Shader> &shader)
    {
        for (MeshNode &node : m_Scene.nodes)
        {
            for (const Ref<MeshInstance> &meshInstance : node.meshInstances)
            {
                shader->SetUniform("u_Model", m_Transform * meshInstance->localTransform);
                meshInstance->mesh->vertexArray->Bind();
                Renderer::DrawIndexed(meshInstance->mesh->vertexArray);
            }
        }
    }

    void Model::SetTransform(const glm::mat4 &transform)
    {
        m_Transform = transform;
    }

    Ref<Model> Model::Create(const std::string &filename)
    {
        return std::make_shared<Model>(filename);
    }
}