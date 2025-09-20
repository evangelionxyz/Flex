#version 460 core

layout (location = 0) out vec4 fragColor;
layout (location = 0) in vec2 vUV;

layout (binding = 0) uniform sampler2D u_ColorTexture;
layout (binding = 1) uniform sampler2D u_DepthTexture;
layout (binding = 3) uniform sampler2D u_BloomTexHQ; // High quality final bloom
layout (binding = 8) uniform sampler2D u_AOTexture; // SSAO (optional)

#define RENDER_MODE_DEPTH 4
#define UNIFORM_BINDING_LOC_SCENE 1
layout (std140, binding = UNIFORM_BINDING_LOC_SCENE) uniform Scene
{
    vec4 lightColor; // w = intensity
    vec2 lightAngle; // x = azimuth, y = elevation
    float renderMode;
    float fogDensity;
    vec4 fogColor;
    float fogStart;
    float fogEnd;
    float padding[2];
} u_Scene;

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
uniform bool u_EnableSSAO;
uniform float u_AOIntensity; // blend strength
uniform bool u_DebugSSAO;

// Vignette uniforms
uniform float u_VignetteRadius;
uniform float u_VignetteSoftness;
uniform float u_VignetteIntensity;
uniform vec3 u_VignetteColor;

// Chromatic aberration uniforms
uniform float u_ChromaticAberrationAmount;
uniform float u_ChromaticAberrationRadial;

float LinearizeDepth(float depth, float near, float far)
{
    float z = depth * 2.0 - 1.0; // Convert to NDC
    return (2.0 * near * far) / (far + near - z * (far - near));
}

vec3 SampleColorWithChromAb(sampler2D tex, vec2 uv, float amount, float radial)
{
    vec2 center = vec2(0.5);
    vec2 direction = uv - center;
    float distance = length(direction);
    
    // Scale chromatic aberration by distance from center
    float aberration = amount * (1.0 + radial * distance * distance);
    
    // Sample each channel with slight offset
    float r = texture(tex, uv + direction * aberration * 0.5).r;
    float g = texture(tex, uv).g;
    float b = texture(tex, uv - direction * aberration * 0.5).b;
    
    return vec3(r, g, b);
}

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

vec3 ApplyFog(vec3 color, float depth, vec2 uv, mat4 inverseProjection, float fogDensity, vec4 fogColor, float fogStart, float fogEnd)
{
    if (fogDensity <= 0.0)
        return color;
        
    // Convert depth to view space distance
    vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewSpace = inverseProjection * clipSpace;
    viewSpace /= viewSpace.w;
    
    float viewDistance = abs(viewSpace.z);
    float fogFactor = 1.0;
    
    // 1. Linear fog for base distance fade
    float linearFog = clamp((viewDistance - fogStart) / (fogEnd - fogStart), 0.0, 1.0);
    
    // 2. Exponential fog for atmospheric depth
    float expFog = 1.0 - exp(-fogDensity * viewDistance);
    
    // 3. Exponential squared fog for dense atmosphere
    float exp2Fog = 1.0 - exp(-pow(fogDensity * viewDistance * 0.5, 2.0));
    
    // 4. Height-based fog simulation (assuming ground plane at y=0)
    // Simulate fog density variation with height
    vec2 ndcPos = uv * 2.0 - 1.0;
    float heightFactor = max(0.0, 1.0 - abs(ndcPos.y) * 0.3); // More fog at horizon
    
    // Combine fog models with weighted blending
    float combinedFog = mix(linearFog, expFog, 0.6) + exp2Fog * 0.3;
    combinedFog *= (1.0 + heightFactor * 0.4); // Enhance horizon fog
    
    // Distance-based fog color variation (cooler colors at distance)
    vec3 distantFogColor = mix(fogColor.rgb, fogColor.rgb * vec3(0.8, 0.9, 1.1), clamp(viewDistance / fogEnd, 0.0, 1.0));
    
    // Atmospheric perspective (slight blue shift at distance)
    float atmosphericStrength = clamp(viewDistance / (fogEnd * 2.0), 0.0, 1.0);
    vec3 atmosphericColor = mix(distantFogColor, distantFogColor * vec3(0.7, 0.8, 1.2), atmosphericStrength * 0.3);
    
    fogFactor = 1.0 - clamp(combinedFog, 0.0, 1.0);
    return mix(atmosphericColor, color, fogFactor);
}

void main()
{
    vec2 uv = vUV;
    float depth = texture(u_DepthTexture, uv).r;
    
    // Convert depth from NDC to view space
    vec4 clipSpace = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 viewSpace = u_InverseProjection * clipSpace;
    viewSpace /= viewSpace.w;
    float dist = abs(viewSpace.z); // Use abs to ensure positive distance

    if (int(u_Scene.renderMode) == RENDER_MODE_DEPTH)
    {
        fragColor = vec4(depth, depth, depth, 1.0);
    }
    else
    {
        vec4 baseColor = texture(u_ColorTexture, uv);
        // Apply SSAO (multiply diffuse) before DOF/fog when still linear HDR
        if (u_EnableSSAO)
        {
            float ao = texture(u_AOTexture, uv).r;
            ao = clamp(ao, 0.0, 1.0);
            if (u_DebugSSAO)
            {
                fragColor = vec4(vec3(ao), 1.0);
                return;
            }
            float blendAO = mix(1.0, ao, u_AOIntensity);
            baseColor.rgb *= blendAO;
        }

        if (u_EnableChromAb)
        {
            vec2 center = vec2(0.5);
            vec2 dir = uv - center;
            float distUV = length(dir);
            float amt = u_ChromaticAberrationAmount * pow(distUV, u_ChromaticAberrationRadial);
            vec2 offset = dir * amt;
            float r = texture(u_ColorTexture, uv + offset).r; // sample shifted
            float g = baseColor.g;                            // keep green reference
            float b = texture(u_ColorTexture, uv - offset).b; // opposite shift
            baseColor.rgb = vec3(r, g, b);
        }

        if (u_EnableDOF)
        {
            // Calculate circle of confusion (CoC)
            float distanceFromFocus = abs(dist - u_FocalDistance);
            float coc = 0.0;
            if (distanceFromFocus > u_FocusRange)
            {
                coc = (distanceFromFocus - u_FocusRange) * u_FocalLength / (u_FStop * max(dist, 0.1));
            }
            coc = min(coc, u_BlurAmount);

            if (coc >= 0.1)
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
                            vec3 sampleColor = texture(u_ColorTexture, suv).rgb;
                            float sampleDepth = texture(u_DepthTexture, suv).r;
                            sampleColor = ApplyFog(sampleColor, sampleDepth, suv, u_InverseProjection, u_Scene.fogDensity, u_Scene.fogColor, u_Scene.fogStart, u_Scene.fogEnd);
                            accum += vec4(sampleColor, 1.0);
                            totalWeight += 1.0;
                        }
                    }
                }
                if (totalWeight > 0.0)
                    baseColor = accum / totalWeight;
            }
            else
            {
                // No blur, apply fog to center
                baseColor.rgb = ApplyFog(baseColor.rgb, depth, uv, u_InverseProjection, u_Scene.fogDensity, u_Scene.fogColor, u_Scene.fogStart, u_Scene.fogEnd);
            }
        }
        else
        {
            // No DOF, apply fog to center
            baseColor.rgb = ApplyFog(baseColor.rgb, depth, uv, u_InverseProjection, u_Scene.fogDensity, u_Scene.fogColor, u_Scene.fogStart, u_Scene.fogEnd);
        }

        // Bloom composite BEFORE tone mapping (HDR domain)
        if (u_EnableBloom)
        {
            baseColor.rgb += texture(u_BloomTexHQ, uv).rgb;
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

        vec3 mapped = FilmicTonemap(baseColor.rgb, u_Exposure, u_Gamma);
        fragColor = vec4(mapped, 1.0);
    }
}
