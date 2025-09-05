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

vec3 SchlickFresnel(float cosTheta, vec3 specularColor)
{
    return specularColor + (1.0 - specularColor) * pow(1.0 - cosTheta, 5.0);
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

vec3 GGX(vec3 normal, vec3 lightDirection, vec3 viewDirection, vec3 lightIrradiance, vec3 diffuseColor, vec3 specularColor, float roughness)
{
    vec3 halfVector = normalize(viewDirection + lightDirection);
    
    // Fresnel
    float cosTheta = max(dot(viewDirection, halfVector), 0.0);
    vec3 F = SchlickFresnel(cosTheta, specularColor);
    
    // Normal Distribution Function
    float D = TRDistribution(normal, halfVector, roughness);
    
    // Geometric Shadowing
    float V = GGXVisiblity(normal, lightDirection, viewDirection, roughness);
    
    // Specular BRDF
    vec3 specular = (F * D * V) / (4.0 * max(dot(normal, lightDirection), 0.0) * max(dot(normal, viewDirection), 0.0) + 0.001);
    
    // Diffuse BRDF (Lambertian)
    vec3 diffuse = (1.0 - F) * diffuseColor * M_RCPPI;
    
    vec3 brdf = diffuse + specular;
    brdf *= max(dot(normal, lightDirection), 0.0) * lightIrradiance;
    
    return brdf;
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
    // Sample normal map
    vec3 normalMapValue = texture(normalMap, uv).rgb;
    // Convert from [0,1] to [-1,1]
    normalMapValue = normalMapValue * 2.0 - 1.0;
    
    // Create TBN matrix
    mat3 TBN = mat3(tangent, bitangent, normal);
    
    // Transform normal from tangent space to world space
    return normalize(TBN * normalMapValue);
}

layout (location = 0) out vec4 fragColor;
layout (std140, binding = 0) uniform Camera
{
    mat4 viewProjection;
    vec4 position;
} u_Camera;

#define RENDER_MODE_COLOR 0
#define RENDER_MODE_NORMALS 1
#define RENDER_MODE_METALLIC 2
#define RENDER_MODE_ROUGHNESS 3

layout (std140, binding = 1) uniform Debug
{
    int renderMode;
} u_Debug;

layout (std140, binding = 2) uniform Time
{
    float time;
} u_Time;

struct VERTEX
{
    vec3 worlPosition;
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

void main()
{
    vec3 normals = normalize(_input.normals.xyz);
    vec3 tangent = normalize(_input.tangent);
    vec3 bitangent = normalize(_input.bitangent);
    vec3 viewDirection = normalize(u_Camera.position.xyz - _input.worlPosition);
    
    // Rotate sun around the object based on time
    float sunRotationSpeed = 0.5; // Adjust speed as needed
    float angle = u_Time.time * sunRotationSpeed;
    vec3 sunDirection = vec3(cos(angle), 0.3, sin(angle)); // Sun moves in a circle with slight elevation
    vec3 lightDirection = normalize(-sunDirection);
    
    vec3 lightColor = vec3(1.0);
    float lightIntensity = 0.3;

    float sunAngularRadius = 0.05;
    float sunSolidAngle = 2.0 * M_PI * (1.0 - cos(sunAngularRadius)); // steradians

    vec3 reflectDirection = reflect(-viewDirection, normals);

    vec3 baseColorTex = texture(u_BaseColorTexture, _input.uv).rgb;
    vec3 emissiveColorTex = texture(u_EmissiveTexture, _input.uv).rgb;
    vec3 metallicRoughnessColorTex = texture(u_MetallicRoughnessTexture, _input.uv).rgb;
    vec3 normalMapTex = texture(u_NormalTexture, _input.uv).rgb;
    float occlusionTex = texture(u_OcclusionTexture, _input.uv).r;
    float metallic = metallicRoughnessColorTex.b;
    float roughness = metallicRoughnessColorTex.g;

    if (u_Debug.renderMode == RENDER_MODE_COLOR)
    {
        float minRoughness = clamp(sqrt(sunSolidAngle / M_PI) * 0.5, 0.0, 1.0);
        float filteredRoughness = max(roughness, minRoughness);

        vec3 diffuseColor = baseColorTex * (1.0 - metallic);
        vec3 specularColor = mix(vec3(0.04), baseColorTex, metallic);
        
        // Use normal mapping if available
        vec3 finalNormal = normals;
        if (length(normalMapTex) > 0.01) // Check if normal map has meaningful data
        {
            finalNormal = GetNormalFromMap(normals, tangent, bitangent, _input.uv, u_NormalTexture);
        }
        
        vec3 reflectRadiance = SampleSphericalMap(u_EnvironmentTexture, reflectDirection);
        reflectRadiance = reflectRadiance / (reflectRadiance + 1.0f);

        vec3 ambient = diffuseColor * vec3(0.6) * occlusionTex;
        
        vec3 irradiance = lightColor * lightIntensity;
        vec3 directLighting = GGX(
            finalNormal,
            lightDirection,
            viewDirection,
            irradiance, 
            diffuseColor,
            specularColor,
            filteredRoughness
        );

        float cosTheta_env = max(dot(viewDirection, finalNormal), 0.0);
        vec3 F_env = SchlickFresnel(cosTheta_env, specularColor);
        float NdotR = max(dot(finalNormal, reflectDirection), 0.0);
        vec3 envSpecular = reflectRadiance * F_env * NdotR;
        
        fragColor = vec4(directLighting + ambient + envSpecular, 1.0);

        // Add emissive if present
        if (any(greaterThan(emissiveColorTex, vec3(0.01))))
        {
            fragColor.rgb += emissiveColorTex;
        }
    }
    else if (u_Debug.renderMode == RENDER_MODE_NORMALS)
    {
        vec3 n = normals * 0.5 + 0.5;
        vec3 finalNormal = n;
        if (length(normalMapTex) > 0.01) // Check if normal map has meaningful data
        {
            finalNormal = GetNormalFromMap(n, tangent, bitangent, _input.uv, u_NormalTexture);
        }
        fragColor = vec4(finalNormal, 1.0);
    }
    else if (u_Debug.renderMode == RENDER_MODE_METALLIC)
    {
        vec4 metallicRoughnessColorTex = texture(u_MetallicRoughnessTexture, _input.uv);
        float metallic = metallicRoughnessColorTex.b;
        fragColor = vec4(metallic, metallic, metallic, 1.0);
    }
    else if (u_Debug.renderMode == RENDER_MODE_ROUGHNESS)
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