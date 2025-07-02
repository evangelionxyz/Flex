#version 410 core
layout (location = 0) out vec4 fragColor;

struct VERTEX
{
    vec3 position;
    vec3 color;
    vec2 texCoord;
};

layout (location = 0) in VERTEX inVertex;

void main()
{
    fragColor = vec4(inVertex.color, 1.0f);
}