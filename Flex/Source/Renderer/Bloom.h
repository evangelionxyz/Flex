// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef BLOOM_H
#define BLOOM_H

#include <vector>
#include <memory>
#include "Framebuffer.h"
#include "Shader.h"

namespace flex
{
    struct BloomSettings
    {
        int iterations = 6;        // More levels for higher quality
        float threshold = 1.0f;    // HDR threshold
        float knee = 0.5f;         // Soft knee for smooth transition
        float radius = 1.0f;       // Blur radius multiplier
        float intensity = 1.0f;    // Final bloom intensity
    };

    class Bloom
    {
    public:
        Bloom(int width, int height);
        ~Bloom();

        void Resize(int width, int height);
        void Build(uint32_t sourceTex);
        void BindTextures() const;
        
        // Get the final bloom texture for combining
        uint32_t GetBloomTexture() const;
        
        BloomSettings settings;
    private:
        void CreateMipFramebuffers(int width, int height);

        struct Level
        {
            int width = 0;
            int height = 0;
            // Three framebuffers for high-quality bloom
            std::shared_ptr<Framebuffer> fbDown;    // Downsampled result
            std::shared_ptr<Framebuffer> fbBlurH;   // Horizontal blur result
            std::shared_ptr<Framebuffer> fbBlurV;   // Final vertical blur result
        };

        std::vector<Level> m_Levels;           // Bloom mip chain levels
        std::shared_ptr<Framebuffer> m_FinalFB; // Final upsampled result
        
        Shader m_DownsampleShader;
        Shader m_BlurShader;
        Shader m_UpsampleShader;
        uint32_t m_Vao;
        
        int m_Width, m_Height;
    };
}

#endif