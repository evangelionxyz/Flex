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
    vec3 worldPosition;
    vec3 position;
    vec3 normals;
    vec3 tangent;
    vec3 bitangent;
    vec3 color;
    vec2 uv;
};

layout (location = 0) out VERTEX _output;

void main()
{
    // World position with translation
    _output.worldPosition = (u_Transform * vec4(position, 1.0)).xyz;
    _output.position = position;
    // Transform normals to world space (approximate; assumes uniform scale)
    _output.normals = normalize(mat3(u_Transform) * normals);
    _output.tangent = normalize(mat3(u_Transform) * tangent);
    _output.bitangent = normalize(mat3(u_Transform) * bitangent);
    _output.color = color;
    _output.uv = uv;

    gl_Position = u_Camera.viewProjection * u_Transform * vec4(position, 1.0);
}
