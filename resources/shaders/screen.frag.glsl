#version 460 core

layout (location = 0) out vec4 fragColor;

struct VERTEX
{
    vec2 uv;
};

layout (location = 0) in VERTEX _input;
layout (binding = 0) uniform sampler2D texture0;

void main()
{
    fragColor = texture(texture0, _input.uv);
}