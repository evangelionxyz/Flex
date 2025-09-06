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
// Post effect toggles
uniform bool u_EnableDOF;
uniform bool u_EnableVignette;
uniform bool u_EnableChromAb;

// Vignette parameters
uniform float vignetteRadius;      // 0..1 (distance from center where vignette starts)
uniform float vignetteSoftness;    // 0..1 (blend region size)
uniform float vignetteIntensity;   // 0..2 (strength)
uniform vec3  vignetteColor;       // Tint color (usually black)

// Chromatic aberration parameters
uniform float chromAbAmount;       // e.g. 0..0.02
uniform float chromAbRadial;       // power falloff factor

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
    
    vec4 baseColor = texture(texture0, uv);

    if (u_EnableDOF)
    {
        // Calculate circle of confusion (CoC)
        float distanceFromFocus = abs(dist - focalDistance);
        float coc = 0.0;
        if (distanceFromFocus > focusRange)
        {
            coc = (distanceFromFocus - focusRange) * focalLength / (fStop * max(dist, 0.1));
        }
        coc = min(coc, maxBlur);

        if (coc >= 0.001)
        {
            vec4 accum = vec4(0.0);
            float totalWeight = 0.0;
            int samples = 15; // balance quality/perf
            for (int i = -samples/2; i <= samples/2; ++i)
            {
                for (int j = -samples/2; j <= samples/2; ++j)
                {
                    vec2 offset = vec2(i, j) * coc / vec2(textureSize(texture0, 0));
                    vec2 suv = uv + offset;
                    if (all(greaterThanEqual(suv, vec2(0.0))) && all(lessThanEqual(suv, vec2(1.0))))
                    {
                        accum += texture(texture0, suv);
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
        float amt = chromAbAmount * pow(distUV, chromAbRadial);
        vec2 offset = dir * amt;
        float r = texture(texture0, uv + offset).r; // sample shifted
        float g = baseColor.g;                      // keep green reference
        float b = texture(texture0, uv - offset).b; // opposite shift
        baseColor.rgb = vec3(r, g, b);
    }

    // Vignette AFTER chromatic aberration but still linear
    if (u_EnableVignette)
    {
        float rad = clamp(vignetteRadius, 0.0, 1.0);
        float soft = clamp(vignetteSoftness, 0.0001, 1.0);
        vec2 pos = uv - 0.5;
        float distNorm = length(pos) / 0.70710678; // normalize so corners ~=1
        
        float vig = 1.0 - smoothstep(rad, rad - soft, distNorm);

        float intensity = vignetteIntensity;
        baseColor.rgb = mix(baseColor.rgb, baseColor.rgb * vignetteColor, vig * intensity);
    }

    // Tone mapping + gamma
    vec3 mapped = FilmicTonemap(baseColor.rgb, exposure, gamma);
    fragColor = vec4(mapped, 1.0);
}