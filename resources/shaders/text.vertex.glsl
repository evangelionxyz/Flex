#version 410 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
layout (location = 2) in vec2 texCoord;
layout (location = 3) in int textureIndex;

uniform mat4 viewProjection;

struct VERTEX
{
    vec2 texCoord;
    vec3 color;
};

layout (location = 0) flat out int outTexIndex;
layout (location = 1) out VERTEX _output;

void main()
{
    outTexIndex = int(textureIndex);
    _output.texCoord = texCoord;
    _output.color = color;
    
    gl_Position = viewProjection * vec4(position, 1.0);
}
