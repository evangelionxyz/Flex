#include "Model.h"

#include "Renderer/Material.h"

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
        for (const std::shared_ptr<Mesh> &mesh : node.meshes)
        {
            
        }
    }
}

void Model::Render(Shader &shader, const std::shared_ptr<Texture2D> &environmentTexture)
{
    for (MeshNode &node : m_Scene.nodes)
    {
        for (const std::shared_ptr<Mesh> &mesh : node.meshes)
        {
            if (mesh->material)
            {
                // Apply material parameters
                m_MaterialUbo->SetData(&mesh->material->params, sizeof(Material::Params));

                mesh->material->occlusionTexture->Bind(4);
                shader.SetUniform("u_OcclusionTexture", 4);
    
                mesh->material->normalTexture->Bind(3);
                shader.SetUniform("u_NormalTexture", 3);
    
                mesh->material->metallicRoughnessTexture->Bind(2);
                shader.SetUniform("u_MetallicRoughnessTexture", 2);
    
                mesh->material->emissiveTexture->Bind(1);
                shader.SetUniform("u_EmissiveTexture", 1);
    
                mesh->material->baseColorTexture->Bind(0);
                shader.SetUniform("u_BaseColorTexture", 0);
            }
    
            // Bind environment last to guarantee it stays on unit 5
            environmentTexture->Bind(5);
            shader.SetUniform("u_EnvironmentTexture", 5);
            
            // TODO: Calculate with mesh transform
            shader.SetUniform("u_Transform", m_Transform * mesh->localTransform);
    
            mesh->vertexArray->Bind();
            Renderer::DrawIndexed(mesh->vertexArray);
        }
    }
}

void Model::SetTransform(const glm::mat4 &transform)
{
    m_Transform = transform;
}

std::shared_ptr<Model> Model::Create(const std::string &filename)
{
    return std::make_shared<Model>(filename);
}
