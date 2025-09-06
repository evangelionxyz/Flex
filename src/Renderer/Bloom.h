#pragma once
#include <vector>
#include <memory>
#include "Framebuffer.h"
#include "Shader.h"

struct BloomSettings
{
    int iterations = 5;
};

class Bloom
{
public:
    void Init(int width, int height);
    void Resize(int width, int height);
    void Build(uint32_t sourceTex, float threshold, float knee, int iterations);
    void BindTextures() const;
private:
    void CreateMipFramebuffers(int width, int height);

    struct Level
    {
        int width = 0;
        int height = 0;
        std::shared_ptr<Framebuffer> fb;
    };

    std::vector<Level> m_Levels; // downsampled/blurred
    Shader m_DownsampleShader;
    Shader m_BlurShader;
};
