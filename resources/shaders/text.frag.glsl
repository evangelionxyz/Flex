#version 410 core
layout (location = 0) out vec4 fragColor;
uniform sampler2D texture0;

struct VERTEX
{
    vec2 texCoord;
    vec3 color;
};

layout (location = 0) in VERTEX inVertex;

void main()
{
    vec4 texColor = vec4(vec3(1.0), texture(texture0, inVertex.texCoord).r);
    fragColor = vec4(inVertex.color, 1.0) * texColor;
}
