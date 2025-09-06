#version 460

// PBR Functions
#define M_RCPPI 0.31830988618379067153776752674503
#define M_PI 3.1415926535897932384626433832795

vec3 LightFallOff(vec3 lightIntensity, vec3 fallOff, vec3 lightPosition, vec3 position)
{
    float dist = distance(lightPosition, position);
    float fo = fallOff.x + (fallOff.y * dist) + (fallOff.z * dist * dist);
    return lightIntensity / fo;
}

vec3 SchlickFresnel(vec3 lightDirection, vec3 normal, vec3 specularColor)
{
    float LH = dot(lightDirection, normal);
    return specularColor + (1.0 - specularColor) * pow(1.0 - LH, 5.0);
}

float TRDistribution(vec3 normal, vec3 halfVector, float roughness)
{
    float NSq = roughness * roughness;
    float NH = max(dot(normal, halfVector), 0.0);
    float denom = NH * NH * (NSq - 1.0) + 1.0;
    return NSq / (M_PI * denom * denom);
}

float GGXVisiblity(vec3 normal, vec3 lightDirection, vec3 viewDirection, float roughness)
{
    float NL = max(dot(normal, lightDirection), 0.05);
    float NV = max(dot(normal, viewDirection), 0.05);
    float RSq = roughness * roughness;
    float RMod = 1.0 - RSq;
    float recipG1 = NL * sqrt(RSq + (RMod * NL * NL));
    float recipG2 = NV * sqrt(RSq + (RMod * NV * NV));
    return 1.0 / (recipG1 * recipG2);
}

vec3 GGXReflect(vec3 normal, vec3 reflectDirection, vec3 viewDirection, in vec3 reflectRadiance, vec3 specularColor, float roughness)
{
    vec3 F = SchlickFresnel(reflectDirection, normal, specularColor);
    float V = GGXVisiblity(normal, reflectDirection, viewDirection, roughness);
    vec3 retColor = F * V;
    retColor *= 4.0 * dot(viewDirection, normal);
    retColor *= max(dot(normal, reflectDirection), 0.0);
    retColor *= reflectRadiance;
    return retColor;
}

vec3 GGX(vec3 normal, vec3 lightDirection, vec3 viewDirection, vec3 lightIrradiance, vec3 diffuseColor, vec3 specularColor, float roughness)
{
    vec3 diffuse = diffuseColor * M_RCPPI;
    vec3 halfVector = normalize(viewDirection + lightDirection);
    vec3 F = SchlickFresnel(lightDirection, halfVector, specularColor);
    float D = TRDistribution(normal, halfVector, roughness);
    float V = GGXVisiblity(normal, lightDirection, viewDirection, roughness);

    vec3 retColor = diffuse + (F * D * V);
    retColor *= max(dot(normal, lightDirection), 0.0);
    retColor *= lightIrradiance;
    return retColor;
}

vec3 SampleSphericalMap(sampler2D tex, vec3 dir)
{
    dir = normalize(dir);
    vec2 uv;
    uv.x = atan(dir.z, dir.x) / (2.0 * 3.14159265) + 0.5;
    uv.y = asin(clamp(dir.y, -1.0, 1.0)) / 3.14159265 + 0.5;
    uv.y = 1.0 - uv.y;
    return texture(tex, uv).rgb;
}

vec3 GetNormalFromMap(vec3 normal, vec3 tangent, vec3 bitangent, vec2 uv, sampler2D normalMap)
{
    vec3 normalMapValue = texture(normalMap, uv).rgb;
    normalMapValue = normalMapValue * 2.0 - 1.0;
    mat3 TBN = mat3(tangent, bitangent, normal);
    return normalize(TBN * normalMapValue);
}

#define RENDER_MODE_COLOR 0
#define RENDER_MODE_NORMALS 1
#define RENDER_MODE_METALLIC 2
#define RENDER_MODE_ROUGHNESS 3

#define UNIFORM_BINDING_LOC_CAMERA 0
#define UNIFORM_BINDING_LOC_SCENE 1
#define UNIFORM_BINDING_LOC_MATERIAL 2

layout (location = 0) out vec4 fragColor;
layout (std140, binding = UNIFORM_BINDING_LOC_CAMERA) uniform Camera
{
    mat4 viewProjection;
    vec4 position;
} u_Camera;

layout (std140, binding = UNIFORM_BINDING_LOC_SCENE) uniform Scene
{
    float time;
    int renderMode;
} u_Scene;

layout (std140, binding = UNIFORM_BINDING_LOC_MATERIAL) uniform Material
{
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float occlusionStrength;
} u_Material;

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

layout (location = 0) in VERTEX _input;

layout (binding = 0) uniform sampler2D u_BaseColorTexture;
layout (binding = 1) uniform sampler2D u_EmissiveTexture;
layout (binding = 2) uniform sampler2D u_MetallicRoughnessTexture;
layout (binding = 3) uniform sampler2D u_NormalTexture;
layout (binding = 4) uniform sampler2D u_OcclusionTexture;
layout (binding = 5) uniform sampler2D u_EnvironmentTexture;

uniform vec3 u_LightColor = vec3(0.5);
uniform float u_LightIntensity = 0.3;

void main()
{
    vec3 normals = normalize(_input.normals.xyz);
    vec3 tangent = normalize(_input.tangent);
    vec3 bitangent = normalize(_input.bitangent);
    vec3 viewDirection = normalize(u_Camera.position.xyz - _input.worldPosition);
    
    // Rotate sun around the object based on time
    float sunRotationSpeed = 0.8; // Adjust speed as needed
    float angle = u_Scene.time * sunRotationSpeed;
    vec3 sunDirection = vec3(cos(angle), 0.3, sin(angle)); // Sun moves in a circle with slight elevation
    vec3 lightDirection = normalize(-sunDirection);
    vec3 reflectDirection = reflect(-viewDirection, normals);

    float sunAngularRadius = 0.05;
    float sunSolidAngle = 2.0 * M_PI * (1.0 - cos(sunAngularRadius)); // steradians

    vec3 baseColorTex = texture(u_BaseColorTexture, _input.uv).rgb;
    vec3 emissiveColorTex = texture(u_EmissiveTexture, _input.uv).rgb * u_Material.emissiveFactor.rgb;
    vec3 metallicRoughnessColorTex = texture(u_MetallicRoughnessTexture, _input.uv).rgb;
    vec3 normalMapTex = texture(u_NormalTexture, _input.uv).rgb;
    float occlusionTex = texture(u_OcclusionTexture, _input.uv).r;
    float metallicVal = metallicRoughnessColorTex.b * u_Material.metallicFactor;
    float roughnessTex = metallicRoughnessColorTex.g;
    float roughnessVal = clamp(roughnessTex * (1.0 - u_Material.roughnessFactor), 0.0, 1.0);

    // Detect if occlusion texture is actually a white fallback (heuristic)
    occlusionTex = abs(occlusionTex - 1.0) < 0.0001 
        ? 1.0 * u_Material.occlusionStrength
        : occlusionTex * u_Material.occlusionStrength;

    if (u_Scene.renderMode == RENDER_MODE_COLOR)
    {
        // Use user/texture roughness directly (already clamped). Removed sunSolidAngle filtering for clearer control.
        vec3 diffuseColor = baseColorTex * _input.color * u_Material.baseColorFactor.rgb * (1.0 - metallicVal);
        vec3 specularColor = mix(vec3(0.04), baseColorTex, metallicVal);
        
        // Use normal mapping if available
        vec3 finalNormal = normals;
        if (length(normalMapTex) > 0.01) // Check if normal map has meaningful data
            finalNormal = GetNormalFromMap(normals, tangent, bitangent, _input.uv, u_NormalTexture);
        
        vec3 reflectRadiance = SampleSphericalMap(u_EnvironmentTexture, reflectDirection);
        reflectRadiance = reflectRadiance / (reflectRadiance + 1.0);

        float reflectionStrength = mix(0.001, 1.0, metallicVal) * (1.0 - roughnessVal);
        vec3 F = SchlickFresnel(viewDirection, finalNormal, specularColor);
        float NdotR = clamp(dot(finalNormal, reflectDirection), 0.0, 1.0);
        vec3 reflectedSpecular = GGXReflect(finalNormal, 
            viewDirection, 
            reflectDirection, 
            reflectRadiance, 
            specularColor, 
            roughnessVal
        ) * reflectionStrength * NdotR * F;

        vec3 ambient = diffuseColor * vec3(0.4) * occlusionTex;
        vec3 irradiance = u_LightColor * u_LightIntensity;
        vec3 directLighting = GGX(finalNormal,
            lightDirection,
            viewDirection,
            irradiance, 
            diffuseColor,
            specularColor,
            roughnessVal
        );
        
        fragColor = vec4(directLighting + ambient + reflectedSpecular, 1.0);

        // Add emissive if present
        // Only add emissive if not effectively black (fallback)
        if (length(emissiveColorTex) >= 0.01)
        {
            fragColor.rgb += emissiveColorTex;
        }
    }
    else if (u_Scene.renderMode == RENDER_MODE_NORMALS)
    {
        vec3 n = normals * 0.5 + 0.5;
        vec3 finalNormal = normals * 0.5 + 0.5;
        if (length(normalMapTex) > 0.01) // Check if normal map has meaningful data
            finalNormal = GetNormalFromMap(n, tangent, bitangent, _input.uv, u_NormalTexture);
        
        fragColor = vec4(finalNormal, 1.0);
    }
    else if (u_Scene.renderMode == RENDER_MODE_METALLIC)
    {
        vec4 metallicRoughnessColorTex = texture(u_MetallicRoughnessTexture, _input.uv);
        float metallic = metallicRoughnessColorTex.b * u_Material.metallicFactor;
        fragColor = vec4(metallic, metallic, metallic, 1.0);
    }
    else if (u_Scene.renderMode == RENDER_MODE_ROUGHNESS)
    {
        vec4 metallicRoughnessColorTex = texture(u_MetallicRoughnessTexture, _input.uv);
        float roughness = metallicRoughnessColorTex.g;
        fragColor = vec4(roughness, roughness, roughness, 1.0);
    }
    else
    {
        vec4 baseColorTex = texture(u_BaseColorTexture, _input.uv);
        fragColor = baseColorTex;
    }
}