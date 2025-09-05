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
uniform float focusRange;
uniform float maxBlur;
uniform float exposure;
uniform float gamma;
uniform mat4 inverseProjection;

vec3 FilmicTonemapColor(vec3 color)
{
    vec3 X = max(vec3(0.0), color - 0.004);
    vec3 result = (X * (6.2 * X + 0.5)) / (X * (6.2 * X + 1.7) + 0.06);
    return pow(result, vec3(2.2, 2.2, 2.2));
}

vec3 FilmicTonemap(vec3 color, float exposure, float gamma)
{
    vec3 mappedColor = FilmicTonemapColor(color * exposure);
    vec3 whiteScale = 1.0 / FilmicTonemapColor(vec3(11.2));
    mappedColor *= whiteScale;
    
    // Gamma correction
    vec3 gammaCorrected = pow(mappedColor, vec3(1.0 / gamma));
    return gammaCorrected.rgb;
}

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
    float distanceFromFocus = abs(dist - focalDistance);
    
    // Only blur if outside the focus range
    float coc = 0.0;
    if (distanceFromFocus > focusRange)
    {
        coc = (distanceFromFocus - focusRange) * focalLength / (fStop * max(dist, 0.1));
    }
    coc = min(coc, maxBlur); // Clamp maximum blur radius
    
    vec4 baseColor;
    if (coc < 0.001)
    {
        baseColor = texture(texture0, uv);
    }
    else
    {
        vec4 color = vec4(0.0);
        float totalWeight = 0.0;
        int samples = 15; // Reduced for better performance
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
        baseColor = (totalWeight > 0.0) ? color / totalWeight : texture(texture0, uv);
    }

    fragColor = vec4(FilmicTonemap(baseColor.rgb, exposure, gamma), 1.0);
}