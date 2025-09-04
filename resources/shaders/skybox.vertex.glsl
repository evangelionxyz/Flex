#version 460

layout (location = 0) in vec3 position;

uniform mat4 u_Transform;

out vec3 TexCoords;

void main()
{
    TexCoords = position;
    
    vec4 pos = u_Transform * vec4(position, 1.0);
    
    // Ensure skybox is always at far plane
    gl_Position = pos.xyww;
}
