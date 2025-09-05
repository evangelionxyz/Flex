#version 410 core
layout (location = 0) out vec4 fragColor;

uniform sampler2D textures[32];

struct VERTEX
{
    vec2 texCoord;
    vec3 color;
};

layout (location = 0) flat in int inTexIndex;
layout (location = 1) in VERTEX _input;

float ScreenPxRange(sampler2D atlas) {
    const float pxRange = 2.0; // set to distance field's pixel range
    vec2 unitRange = vec2(pxRange)/vec2(textureSize(atlas, 0));
    vec2 screenTexSize = vec2(1.0)/fwidth(_input.texCoord);
    return max(0.5*dot(unitRange, screenTexSize), 1.0);
}

float median(float r, float g, float b)
{
   return max(min(r, g), min(max(r, g), b));
}

void main()
{
    vec4 texColor = texture(textures[inTexIndex], _input.texCoord) * vec4(_input.color, 1.0);
    vec3 msd = texture(textures[inTexIndex], _input.texCoord).rgb;
    float screenPxDistance = ScreenPxRange(textures[inTexIndex]) * (median(msd.r, msd.g, msd.b) - 0.5);
    
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
    if (opacity == 0.0)
        discard;

    vec4 background = vec4(0.0);
    fragColor = mix(background, vec4(_input.color, 1.0), opacity);
}
