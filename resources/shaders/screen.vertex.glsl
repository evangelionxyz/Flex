#version 460 core

layout (location = 0) in vec2 position;

struct VERTEX
{
    vec2 uv;
};

layout (location = 0) out VERTEX _output;

void main()
{
    _output.uv = position * 0.5 + 0.5;

    gl_Position = vec4(position, 0.0, 1.0);
}