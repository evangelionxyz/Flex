#pragma once

#include <glm/glm.hpp>
#include <array>
#include <memory>

class UniformBuffer;
class Shader;
class Camera;

enum class CascadedQuality
{
    Low,
    Medium,
    High
};

// Simple cascaded shadow map implementation for a directional light (sun)
class CascadedShadowMap
{
public:
    static constexpr int NumCascades = 4;

    struct GPUData
    {
        glm::mat4 lightViewProj[NumCascades];
        glm::vec4 cascadeSplits;          // view-space distances to end of each cascade
        float shadowStrength = 1.0f;
        float minBias = 0.0f;
        float maxBias = 0.0f;
        float pcfRadius = 0.3f;           // in texels (multiplier)
        float padding[3];
    };

    CascadedShadowMap(CascadedQuality quality);
    ~CascadedShadowMap();

    void Resize(CascadedQuality quality);
    void Update(const Camera &camera, const glm::vec3 &lightDir);
    void BeginCascade(int cascadeIndex);
    void EndCascade();
    void BindTexture(int unit) const;
    void Upload();

    const GPUData &GetData() const { return m_Data; }
    GPUData &GetData() { return m_Data; }
    const CascadedQuality GetQuality() const { return m_Quality; }

private:
    void CreateResources();
    void DestroyResources();
    void ComputeMatrices(const Camera &camera, const glm::vec3 &lightDir);

private:
    unsigned int m_FBO = 0;
    unsigned int m_DepthArray = 0;
    int m_Resolution = 0;
    CascadedQuality m_Quality;
    GPUData m_Data{};
    std::shared_ptr<UniformBuffer> m_UBO; // binding = 3
};
