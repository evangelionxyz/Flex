#version 460 core

layout (location = 0) out vec4 fragColor;

struct VERTEX
{
    vec2 uv;
};

#define RENDER_MODE_DEPTH 4
layout (std140, binding = 1) uniform Debug
{
    int renderMode;
} u_Debug;

layout (location = 0) in VERTEX _input;
layout (binding = 0) uniform sampler2D u_ColorTexture;
layout (binding = 1) uniform sampler2D u_DepthTexture;
// Bloom mip chain (downsampled blurred). We'll bind consecutively after main color & depth.
// 0: original color already bound at binding=0, so bloom starts at binding=2 upward.
layout (binding = 2) uniform sampler2D u_BloomTex0; // 1/2 size
layout (binding = 3) uniform sampler2D u_BloomTex1; // 1/4
layout (binding = 4) uniform sampler2D u_BloomTex2; // 1/8
layout (binding = 5) uniform sampler2D u_BloomTex3; // 1/16
layout (binding = 6) uniform sampler2D u_BloomTex4; // 1/32 (optional)


uniform float u_FocalLength;
uniform float u_FocalDistance;
uniform float u_FStop;
uniform float u_FocusRange;
uniform float u_BlurAmount;
uniform float u_Exposure;
uniform float u_Gamma;

uniform mat4 u_InverseProjection;
// Post effect toggles
uniform bool u_EnableDOF;
uniform bool u_EnableVignette;
uniform bool u_EnableChromAb;
uniform bool u_EnableBloom;
uniform float u_BloomThreshold;
uniform float u_BloomKnee;
uniform float u_BloomIntensity;
uniform int u_BloomIterations; // how many of the chain levels to use

// Vignette parameters
uniform float u_VignetteRadius;      // 0..1 (distance from center where vignette starts)
uniform float u_VignetteSoftness;    // 0..1 (blend region size)
uniform float u_VignetteIntensity;   // 0..2 (strength)
uniform vec3  u_VignetteColor;       // Tint color (usually black)

// Chromatic aberration parameters
uniform float u_ChromaticAbAmount;   // e.g. 0..0.02
uniform float u_ChromaticAbRadual;   // power falloff factor

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
    float depth = texture(u_DepthTexture, uv).r;
    
    // Convert depth from NDC to view space
    vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewSpace = u_InverseProjection * clipSpace;
    viewSpace /= viewSpace.w;
    float dist = abs(viewSpace.z); // Use abs to ensure positive distance

    if (u_Debug.renderMode == RENDER_MODE_DEPTH)
    {
        fragColor = vec4(depth, depth, depth, 1.0);
    }
    else
    {
        vec4 baseColor = texture(u_ColorTexture, uv);

        if (u_EnableDOF)
        {
            // Calculate circle of confusion (CoC)
            float distanceFromFocus = abs(dist - u_FocalDistance);
            float coc = 0.0;
            if (distanceFromFocus > u_FocusRange)
            {
                coc = (distanceFromFocus - u_FocusRange) * u_FocalLength / (u_FStop * max(dist, 0.001));
            }
            coc = min(coc, u_BlurAmount);

            if (coc >= 0.001)
            {
                vec4 accum = vec4(0.0);
                float totalWeight = 0.0;
                int samples = 9; // balance quality/perf
                for (int i = -samples/2; i <= samples/2; ++i)
                {
                    for (int j = -samples/2; j <= samples/2; ++j)
                    {
                        vec2 offset = vec2(i, j) * coc / vec2(textureSize(u_ColorTexture, 0));
                        vec2 suv = uv + offset;
                        if (all(greaterThanEqual(suv, vec2(0.0))) && all(lessThanEqual(suv, vec2(1.0))))
                        {
                            accum += texture(u_ColorTexture, suv);
                            totalWeight += 1.0;
                        }
                    }
                }
                if (totalWeight > 0.0)
                    baseColor = accum / totalWeight;
            }
        }

        // Chromatic aberration (simple radial RGB shift) BEFORE tone mapping in linear
        if (u_EnableChromAb)
        {
            vec2 center = vec2(0.5);
            vec2 dir = uv - center;
            float distUV = length(dir);
            float amt = u_ChromaticAbAmount * pow(distUV, u_ChromaticAbRadual);
            vec2 offset = dir * amt;
            float r = texture(u_ColorTexture, uv + offset).r; // sample shifted
            float g = baseColor.g;                            // keep green reference
            float b = texture(u_ColorTexture, uv - offset).b; // opposite shift
            baseColor.rgb = vec3(r, g, b);
        }

        // Vignette AFTER chromatic aberration but still linear
        if (u_EnableVignette)
        {
            float rad = clamp(u_VignetteRadius, 0.0, 1.0);
            float soft = clamp(u_VignetteSoftness, 0.0001, 1.0);
            vec2 pos = uv - 0.5;
            float distNorm = length(pos) / 0.70710678; // normalize so corners ~=1
            
            float vig = 1.0 - smoothstep(rad, rad - soft, distNorm);

            float intensity = u_VignetteIntensity;
            baseColor.rgb = mix(baseColor.rgb, baseColor.rgb * u_VignetteColor, vig * intensity);
        }

        // Tone mapping + gamma
        // Bloom composite BEFORE tone mapping (HDR domain)
        if (u_EnableBloom)
        {
            // Reconstruct bloom from mip chain additively with weights
            int iters = clamp(u_BloomIterations, 1, 5);
            vec3 bloom = vec3(0.0);
            // Soft threshold (Filmic style)
            float knee = u_BloomThreshold * u_BloomKnee + 1e-4;
            float thresh = u_BloomThreshold;
            vec3 src = baseColor.rgb;
            float brightness = max(max(src.r, src.g), src.b);
            float soft = clamp((brightness - thresh + knee) / (2.0 * knee), 0.0, 1.0);
            float contribution = max(brightness - thresh, 0.0);
            float bloomMask = (contribution + soft) / max(brightness, 1e-4);
            bloomMask = clamp(bloomMask, 0.0, 1.0);

            if (iters >= 1) bloom += texture(u_BloomTex0, uv).rgb;
            if (iters >= 2) bloom += texture(u_BloomTex1, uv).rgb * 0.8;
            if (iters >= 3) bloom += texture(u_BloomTex2, uv).rgb * 0.6;
            if (iters >= 4) bloom += texture(u_BloomTex3, uv).rgb * 0.4;
            if (iters >= 5) bloom += texture(u_BloomTex4, uv).rgb * 0.3;

            baseColor.rgb += bloom * bloomMask * u_BloomIntensity;
        }

        vec3 mapped = FilmicTonemap(baseColor.rgb, u_Exposure, u_Gamma);
        fragColor = vec4(mapped, 1.0);
    }
}
