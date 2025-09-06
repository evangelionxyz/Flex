#version 460 core
layout (location=0) out vec4 fragColor;
layout (binding=0) uniform sampler2D u_Src;
layout (location=0) in vec2 vUV;

// Single-pass 9-tap separable-ish approximation sampling in a small disk
const vec2 OFFSETS[9] = vec2[]
(
    vec2(0,0),
    vec2(1,0), vec2(-1,0),
    vec2(0,1), vec2(0,-1),
    vec2(1,1), vec2(-1,1), vec2(1,-1), vec2(-1,-1)
);
const float WEIGHTS[9] = float[](0.25, 0.125,0.125,0.125,0.125,0.0625,0.0625,0.0625,0.0625);

void main(){
    vec2 texel = 1.0/vec2(textureSize(u_Src,0));
    vec3 color = vec3(0.0);
    for(int i = 0; i < 9; ++i)
    {
        color += texture(u_Src, vUV + OFFSETS[i]*texel).rgb * WEIGHTS[i];
    }

    fragColor = vec4(color, 1.0);
}
