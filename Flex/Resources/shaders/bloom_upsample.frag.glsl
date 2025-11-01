#version 460 core

layout (location = 0) out vec4 fragColor;
layout (binding = 0) uniform sampler2D u_LowRes;
layout (binding = 1) uniform sampler2D u_HighRes;

uniform float u_Radius; // blur radius for this level

layout (location = 0) in vec2 vUV;

void main()
{
    vec2 texel = 1.0 / vec2(textureSize(u_LowRes, 0));
    
    // Bilinear upsampling with 9-tap tent filter
    vec3 sum = vec3(0.0);
    
    // Bounds checking for all samples
    vec2 offsets[9] = vec2[]
    (
        vec2(0.0, 0.0),         // center
        vec2(-texel.x * u_Radius, 0.0),    // left
        vec2( texel.x * u_Radius, 0.0),    // right
        vec2(0.0, -texel.y * u_Radius),    // top
        vec2(0.0,  texel.y * u_Radius),    // bottom
        vec2(-texel.x * u_Radius, -texel.y * u_Radius), // top-left
        vec2( texel.x * u_Radius, -texel.y * u_Radius), // top-right
        vec2(-texel.x * u_Radius,  texel.y * u_Radius), // bottom-left
        vec2( texel.x * u_Radius,  texel.y * u_Radius)  // bottom-right
    );
    
    float weights[9] = float[](4.0, 2.0, 2.0, 2.0, 2.0, 1.0, 1.0, 1.0, 1.0);
    
    for(int i = 0; i < 9; ++i)
    {
        vec2 sampleUV = vUV + offsets[i];
        if (sampleUV.x >= 0.0 && sampleUV.x <= 1.0 && sampleUV.y >= 0.0 && sampleUV.y <= 1.0)
        {
            vec3 sampleColor = texture(u_LowRes, sampleUV).rgb;
            // Clamp samples to reasonable values to prevent inf/NaN
            sampleColor = clamp(sampleColor, vec3(0.0), vec3(100.0));
            sum += sampleColor * weights[i];
        }
    }
    
    vec3 upsampled = sum / 16.0;
    
    // Sample the higher resolution texture with bounds checking
    vec3 highRes = vec3(0.0);
    if (vUV.x >= 0.0 && vUV.x <= 1.0 && vUV.y >= 0.0 && vUV.y <= 1.0)
    {
        highRes = texture(u_HighRes, vUV).rgb;
        // Clamp to prevent inf/NaN
        highRes = clamp(highRes, vec3(0.0), vec3(100.0));
    }
    
    // Combine with additive blending, ensure no negative values or inf/NaN
    vec3 result = clamp(upsampled + highRes, vec3(0.0), vec3(100.0));
    
    fragColor = vec4(result, 1.0);
}
