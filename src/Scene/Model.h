#pragma once

#include "Renderer/Mesh.h"
#include "Renderer/Shader.h"
#include "Renderer/Texture.h"
#include "Renderer/UniformBuffer.h"

#include <string>
#include <memory>
#include <vector>

#include <glm/glm.hpp>

class Model
{
public:
    Model(const std::string &filename);
    ~Model();
    
    void Update(float deltaTime);
    void Render(Shader &shader, const std::shared_ptr<Texture2D> &environmentTexture);
    void RenderDepth(Shader &shader); // depth-only

    void SetTransform(const glm::mat4 &transform);
    const glm::mat4 &GetTransform() { return m_Transform; }

    MeshScene &GetScene() { return m_Scene; }

    static std::shared_ptr<Model> Create(const std::string &filename);

private:
    MeshScene m_Scene;
    std::shared_ptr<UniformBuffer> m_MaterialUbo;
    glm::mat4 m_Transform{};
};