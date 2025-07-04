#version 410 core
layout (location = 0) out vec4 fragColor;

struct VERTEX
{
    vec3 position;
    vec3 color;
    vec2 texCoord;
};

layout (location = 0) in VERTEX inVertex;

uniform sampler2D texture0;

void main()
{
    vec4 texColor = texture(texture0, inVertex.texCoord);
    fragColor = texColor;
}