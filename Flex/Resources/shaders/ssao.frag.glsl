#version 460 core
layout(location=0) out vec4 fragColor;
layout(location=0) in vec2 vUV;

layout(binding=0) uniform sampler2D u_Depth;
layout(binding=1) uniform sampler2D u_Noise;

uniform mat4 u_Projection;      // projection
uniform mat4 u_ProjectionInv;   // inverse projection (precomputed)
uniform float u_Radius;
uniform float u_Bias;
uniform float u_Power;
uniform vec3 u_Samples[32];

// Reconstruct view-space position (right-handed, camera at origin looking -Z)
vec3 ReconstructViewPos(vec2 uv, float depth)
{
    float z = depth * 2.0 - 1.0;          // back to NDC
    vec4 clip = vec4(uv * 2.0 - 1.0, z, 1.0);
    vec4 view = u_ProjectionInv * clip;   // to view
    view /= view.w;
    return view.xyz;                      // view.z is negative forward
}

void main()
{
    float depth = texture(u_Depth, vUV).r;
    if(depth >= 1.0) { fragColor = vec4(1.0); return; }

    vec3 posVS = ReconstructViewPos(vUV, depth);

    // Depth-based normal approximation
    vec2 texel = 1.0 / vec2(textureSize(u_Depth,0));
    float dR = texture(u_Depth, vUV + vec2(texel.x,0)).r;
    float dU = texture(u_Depth, vUV + vec2(0,texel.y)).r;
    vec3 pR = ReconstructViewPos(vUV + vec2(texel.x,0), dR);
    vec3 pU = ReconstructViewPos(vUV + vec2(0,texel.y), dU);
    vec3 dx = pR - posVS;
    vec3 dy = pU - posVS;
    vec3 normal = normalize(cross(dx, dy));
    if (!all(greaterThan(normal, vec3(-10.0)))) normal = normalize(normal); // safety

    // Random rotation
    vec3 rand = texture(u_Noise, vUV * vec2(textureSize(u_Depth,0))/4.0).xyz;
    vec3 tangent = normalize(rand - normal * dot(rand, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occ = 0.0;
    for (int i=0;i<32;i++)
    {
        vec3 sampleVS = TBN * u_Samples[i];
        sampleVS = posVS + sampleVS * u_Radius;
        vec4 clip = u_Projection * vec4(sampleVS,1.0);
        clip.xyz /= clip.w;
        vec2 suv = clip.xy * 0.5 + 0.5;
        if (suv.x < 0.0 || suv.x > 1.0 || suv.y < 0.0 || suv.y > 1.0) continue;
        float sDepth = texture(u_Depth, suv).r;
        vec3 sampleDepthVS = ReconstructViewPos(suv, sDepth);
        float rangeCheck = smoothstep(0.0,1.0, u_Radius / abs(posVS.z - sampleDepthVS.z));
        // Convert to positive distances for comparison
        float centerDist = -posVS.z;
        float sampleDist = -sampleDepthVS.z;
        // If geometry is closer than our sample point (i.e., sample ray hit something sooner) => occlusion
        occ += ((sampleDist < (-sampleVS.z + u_Bias)) ? 1.0 : 0.0) * rangeCheck;
    }
    occ = 1.0 - (occ / 32.0);
    occ = pow(occ, u_Power);
    fragColor = vec4(occ, occ, occ, 1.0);
}
