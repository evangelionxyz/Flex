#version 460 core

layout (location = 0) out vec4 fragColor;
layout (binding = 0) uniform sampler2D u_Src;

uniform int u_Horizontal; // 1 for horizontal, 0 for vertical

layout (location = 0) in vec2 vUV;

// High-quality separable Gaussian blur with 9 taps
// Gaussian weights for sigma = 1.0
const float WEIGHTS[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
    vec2 texel = 1.0 / vec2(textureSize(u_Src, 0));
    vec2 direction = u_Horizontal == 1 ? vec2(texel.x, 0.0) : vec2(0.0, texel.y);
    
    vec3 result = texture(u_Src, vUV).rgb * WEIGHTS[0];
    
    // Sample in both directions
    for(int i = 1; i < 5; ++i)
    {
        vec2 offset = direction * float(i);
        result += texture(u_Src, vUV + offset).rgb * WEIGHTS[i];
        result += texture(u_Src, vUV - offset).rgb * WEIGHTS[i];
    }
    
    fragColor = vec4(result, 1.0);
}
