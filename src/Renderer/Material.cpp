#include "Material.h"

Material::Material()
{
    // Neutral defaults per glTF PBR spec when a texture is absent
    baseColorTexture = Renderer::GetWhiteTexture();           // baseColorFactor will tint
    emissiveTexture = Renderer::GetWhiteTexture();            // no emissive
    metallicRoughnessTexture = Renderer::GetBlackTexture();   // will be overridden if texture present; factors supply values
    normalTexture = Renderer::GetWhiteTexture();              // flat normal
    occlusionTexture = Renderer::GetWhiteTexture();           // full occlusion (no darkening)
}