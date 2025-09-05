#version 460 core

layout (location = 0) out vec4 fragColor;

struct VERTEX
{
    vec2 uv;
};

layout (location = 0) in VERTEX _input;
layout (binding = 0) uniform sampler2D texture0;
layout (binding = 1) uniform sampler2D depthTex;

uniform float focalLength;
uniform float focalDistance;
uniform float fStop;
uniform mat4 inverseProjection;

void main()
{
    vec2 uv = _input.uv;
    float depth = texture(depthTex, uv).r;
    
    // Convert depth from NDC to view space
    vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewSpace = inverseProjection * clipSpace;
    viewSpace /= viewSpace.w;
    float dist = abs(viewSpace.z); // Use abs to ensure positive distance
    
    // Calculate circle of confusion (CoC)
    float focusRange = 5.0;
    float distanceFromFocus = abs(dist - focalDistance);
    
    // Only blur if outside the focus range
    float coc = 0.0;
    if (distanceFromFocus > focusRange)
    {
        coc = (distanceFromFocus - focusRange) * focalLength / (fStop * max(dist, 0.1));
    }
    coc = min(coc, 4.5); // Clamp maximum blur radius
    
    if (coc < 0.001)
    {
        fragColor = texture(texture0, uv);
    }
    else
    {
        vec4 color = vec4(0.0);
        float totalWeight = 0.0;
        int samples = 9; // Reduced for better performance
        
        for(int i = -samples/2; i <= samples/2; i++)
        {
            for(int j = -samples/2; j <= samples/2; j++)
            {
                vec2 offset = vec2(i, j) * coc / vec2(textureSize(texture0, 0));
                vec2 sampleUV = uv + offset;
                
                if(sampleUV.x >= 0.0 && sampleUV.x <= 1.0 && sampleUV.y >= 0.0 && sampleUV.y <= 1.0)
                {
                    color += texture(texture0, sampleUV);
                    totalWeight += 1.0;
                }
            }
        }
        
        if (totalWeight > 0.0)
            fragColor = color / totalWeight;
        else
            fragColor = texture(texture0, uv);
    }
}