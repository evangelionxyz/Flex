#version 460
layout (location = 0) in vec3 aPos;

layout(std140, binding = 3) uniform CascadedShadows
{
    mat4 lightViewProj[4];
    vec4 cascadeSplits;
    float shadowStrength;
    float minBias;
    float maxBias;
    float pcfRadius;
} u_CSM;

uniform mat4 u_Model;
uniform int u_CascadeIndex;

void main()
{
    gl_Position = u_CSM.lightViewProj[u_CascadeIndex] * u_Model * vec4(aPos,1.0);
}
