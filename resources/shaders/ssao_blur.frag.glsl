#version 460 core
layout(location=0) out vec4 fragColor; 
layout(location=0) in vec2 vUV; 
layout(binding=0) uniform sampler2D u_Src; 
void main(){
    vec2 texel = 1.0 / vec2(textureSize(u_Src,0));
    float sum = 0.0; float wsum=0.0; 
    for(int x=-2;x<=2;++x){
        for(int y=-2;y<=2;++y){
            float w = 1.0 - (abs(x)+abs(y))/8.0; 
            sum += texture(u_Src, vUV + vec2(x,y)*texel).r * w; 
            wsum += w; 
        }
    }
    float ao = sum / wsum; 
    fragColor = vec4(ao,ao,ao,1.0); 
}
