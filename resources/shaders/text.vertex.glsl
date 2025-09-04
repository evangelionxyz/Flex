#version 410 core
layout (location = 0) in vec2 position;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec3 color;
layout (location = 3) in int textureIndex;

uniform mat4 viewProjection;

struct VERTEX
{
    vec2 texCoord;
    vec3 color;
};

layout (location = 0) flat out int outTexIndex;
layout (location = 1) out VERTEX outVertex;

void main()
{
    outTexIndex = int(textureIndex);
    outVertex.texCoord = texCoord;
    outVertex.color = color;
    
    gl_Position = viewProjection * vec4(position, 0.0, 1.0);
}
