#version 460

in vec3 TexCoords;
out vec4 FragColor;

uniform sampler2D u_EnvironmentMap;

vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= vec2(0.1591, 0.3183); // 1/(2*PI), 1/PI
    uv += 0.5;
    return uv;
}

void main()
{
    vec2 uv = SampleSphericalMap(normalize(TexCoords));
    uv.y = 1.0 - uv.y;
    vec3 color = texture(u_EnvironmentMap, uv).rgb;
    
    // Simple tone mapping for HDR
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2)); // Gamma correction
    
    FragColor = vec4(color, 1.0);
}
