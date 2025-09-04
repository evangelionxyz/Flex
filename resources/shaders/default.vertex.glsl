#version 460
layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normals;
layout (location = 2) in vec3 tangent;
layout (location = 3) in vec3 bitangent;
layout (location = 4) in vec3 color;
layout (location = 5) in vec2 uv;

layout (std140, binding = 0) uniform Camera
{
    mat4 viewProjection;
    vec4 position;
} u_Camera;

uniform mat4 u_Transform;

struct VERTEX
{
    vec3 worlPosition;
    vec3 position;
    vec3 normals;
    vec3 tangent;
    vec3 bitangent;
    vec3 color;
    vec2 uv;
};

layout (location = 0) out VERTEX outVertex;

void main()
{
    // World position with translation
    outVertex.worlPosition = (u_Transform * vec4(position, 1.0)).xyz;
    outVertex.position = position;
    // Transform normals to world space (approximate; assumes uniform scale)
    outVertex.normals = normalize(mat3(u_Transform) * normals);
    outVertex.tangent = normalize(mat3(u_Transform) * tangent);
    outVertex.bitangent = normalize(mat3(u_Transform) * bitangent);
    outVertex.color = color;
    outVertex.uv = uv;

    gl_Position = u_Camera.viewProjection * u_Transform * vec4(position, 1.0);
}
