#version 460 core
// full screen triangle
layout(location=0) out vec2 vUV;
void main(){
    vec2 pos = vec2((gl_VertexID<<1)&2, gl_VertexID&2);
    vUV = pos; // 0..1
    gl_Position = vec4(pos*2.0-1.0,0,1);
}
