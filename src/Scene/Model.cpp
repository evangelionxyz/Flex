#include "Model.h"

Model::Model(const std::string &filename)
{
    m_Meshes = MeshLoader::LoadFromGLTF(filename);

    m_Transform = glm::mat4(1.0f);
}

Model::~Model()
{
}

void Model::Update(float deltaTime)
{
}

void Model::Render(Shader &shader, std::shared_ptr<Texture2D> &environmentTexture)
{
    for (std::shared_ptr<Mesh> &mesh : m_Meshes)
    {
        if (mesh->material)
        {
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
        shader.SetUniform("u_Transform", m_Transform);

        mesh->vertexArray->Bind();
        Renderer::DrawIndexed(mesh->vertexArray);
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
