#version 460
layout (location = 0) in vec3 aPos;
uniform mat4 u_Model;
layout(std140, binding = 3) uniform CascadedShadows
{
    mat4 u_LightViewProj[4];
    vec4 u_CascadeSplits;
    float u_ShadowStrength; float u_MinBias; float u_MaxBias; float u_PcfRadius;
} u_CSM;
uniform int u_CascadeIndex;
void main(){
    gl_Position = u_CSM.u_LightViewProj[u_CascadeIndex] * u_Model * vec4(aPos,1.0);
}
