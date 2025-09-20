#pragma once
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "Framebuffer.h"
#include "Shader.h"

// Simple SSAO implementation: generates kernel + noise, outputs AO factor texture
class SSAO
{
public:
    SSAO(int width, int height);
    void Resize(int width, int height);

    // Build AO using scene depth & (optionally) normal buffer in future. Currently uses depth only + reconstructed position.
    void Generate(uint32_t depthTex, const glm::mat4 &proj, float radius, float bias, float power);

    // Get final blurred AO for composition
    uint32_t GetAOTexture() const { return m_BlurFB ? m_BlurFB->GetColorAttachment(0) : 0; }

private:
    void CreateFramebuffers(int width, int height);
    void BuildKernel();
    void BuildNoise();

    std::shared_ptr<Framebuffer> m_AOFB;   // raw AO
    std::shared_ptr<Framebuffer> m_BlurFB; // blurred AO

    Shader m_AOShader;
    Shader m_BlurShader;

    std::vector<glm::vec4> m_Kernel; // 4D to keep std140 alignment simple when uploading manually if needed
    uint32_t m_NoiseTex = 0;
    uint32_t m_Vao = 0; // fullscreen triangle VAO
    int m_Width = 0;
    int m_Height = 0;
};
