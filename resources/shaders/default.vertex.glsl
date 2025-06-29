#version 330 core
layout (location = 0) in vec3 position;

vec3 pos[3] = vec3[3]
(
    vec3(-0.5, -0.5, 0.0),
    vec3( 0.0,  0.5, 0.0),
    vec3( 0.5, -0.5, 0.0)
);

void main()
{
    gl_Position = vec4(pos[gl_VertexID], 1.0);
}
