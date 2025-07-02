#version 410 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
layout (location = 2) in vec2 texCoord;

struct VERTEX
{
    vec3 position;
    vec3 color;
    vec2 texCoord;
};

layout (location = 0) out VERTEX outVertex;

void main()
{
    outVertex.position = position;
    outVertex.color = color;
    outVertex.texCoord = texCoord;

    gl_Position = vec4(position, 1.0);
}
