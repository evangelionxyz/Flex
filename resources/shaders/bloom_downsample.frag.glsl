#version 460 core
layout (location=0) out vec4 fragColor;
layout (binding=0) uniform sampler2D u_Src; // set dynamically in code

uniform float u_Threshold; // HDR threshold
uniform float u_Knee;       // soft knee (0..1)

layout (location = 0) in vec2 vUV;

vec3 Prefilter(vec3 c)
{
    float brightness = max(max(c.r,c.g),c.b);
    float knee = u_Threshold * u_Knee + 1e-4;
    float soft = clamp((brightness - u_Threshold + knee)/(2.0*knee),0.0,1.0);
    float contribution = max(brightness - u_Threshold,0.0) + soft;
    contribution /= max(brightness,1e-4);
    return c * contribution;
}

void main(){
    vec2 texel = 1.0/vec2(textureSize(u_Src,0));
    vec3 sum = vec3(0.0);
    // 4 tap box downsample around pixel center
    sum += texture(u_Src, vUV + texel*vec2(-0.5,-0.5)).rgb;
    sum += texture(u_Src, vUV + texel*vec2( 0.5,-0.5)).rgb;
    sum += texture(u_Src, vUV + texel*vec2(-0.5, 0.5)).rgb;
    sum += texture(u_Src, vUV + texel*vec2( 0.5, 0.5)).rgb;
    vec3 c = sum * 0.25;
    fragColor = vec4(Prefilter(c),1.0);
}
