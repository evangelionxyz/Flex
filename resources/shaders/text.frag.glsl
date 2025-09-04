#version 410 core
layout (location = 0) out vec4 fragColor;

uniform sampler2D textures[32];

struct VERTEX
{
    vec2 texCoord;
    vec3 color;
};

layout (location = 0) flat in int inTexIndex;
layout (location = 1) in VERTEX inVertex;

void main()
{
    float alpha = texture(textures[inTexIndex], inVertex.texCoord).r;
    
    // Use a small threshold to avoid rendering very faint pixels
    if (alpha < 0.01)
        discard;
    
    // Modulate both color and alpha by the texture value
    fragColor = vec4(inVertex.color * alpha, alpha);
}
