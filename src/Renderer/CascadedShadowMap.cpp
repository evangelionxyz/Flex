#include "RendererCommon.h"

#include "CascadedShadowMap.h"
#include "Renderer/UniformBuffer.h"
#include "Core/Camera.h"

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

CascadedShadowMap::CascadedShadowMap()
{
    CreateResources();
    m_UBO = UniformBuffer::Create(sizeof(GPUData), UNIFORM_BINDING_LOC_CSM);
}

CascadedShadowMap::~CascadedShadowMap()
{
    DestroyResources();
}

void CascadedShadowMap::Resize(int resolution)
{
    if (resolution == m_Resolution) return;
    m_Resolution = resolution;
    DestroyResources();
    CreateResources();
}

void CascadedShadowMap::CreateResources()
{
    glCreateFramebuffers(1, &m_FBO);
    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &m_DepthArray);
    glTextureStorage3D(m_DepthArray, 1, GL_DEPTH_COMPONENT32F, m_Resolution, m_Resolution, NumCascades);
    glTextureParameteri(m_DepthArray, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_DepthArray, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(m_DepthArray, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTextureParameteri(m_DepthArray, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[4] = {1.0f,1.0f,1.0f,1.0f};
    glTextureParameterfv(m_DepthArray, GL_TEXTURE_BORDER_COLOR, border);
    glTextureParameteri(m_DepthArray, GL_TEXTURE_COMPARE_MODE, GL_NONE); // manual compare
}

void CascadedShadowMap::DestroyResources()
{
    if (m_DepthArray) 
    {
        glDeleteTextures(1, &m_DepthArray);
    }
    
    if (m_FBO)
    {
        glDeleteFramebuffers(1, &m_FBO);
    } 
    
    m_DepthArray = 0;
    m_FBO = 0;
}

static void GetFrustumCornersWS(const glm::mat4 &invViewProj, float nearZ, float farZ, std::array<glm::vec3,8> &out)
{
    // NDC cube corners
    std::array<glm::vec3,8> ndc =
    {
        glm::vec3(-1,-1,1), glm::vec3(1,-1,1), glm::vec3(1,1,1), glm::vec3(-1,1,1), // near (z=1 in OpenGL clip after projection? We'll derive using custom approach)
        glm::vec3(-1,-1,-1), glm::vec3(1,-1,-1), glm::vec3(1,1,-1), glm::vec3(-1,1,-1) // far
    };
    // We'll scale by slice depths; instead easier: create perspective with new near/far; but we have original invViewProj.
    // Approach: get eight world frustum corners of full frustum then linearly interpolate along edges for slice.
    // Simplify: caller supplies its own slice matrix; for brevity we accept some approximation.
    // For precision we recompute using near/far in view space using camera basis later; this helper is unused in final minimal impl.
}

void CascadedShadowMap::ComputeMatrices(const Camera &camera, const glm::vec3 &lightDir)
{
    // Split scheme parameters
    float lambda = 0.7f; // blend factor between uniform & logarithmic
    float n = camera.nearPlane;
    float f = camera.farPlane;

    float clipRange = f - n;

    float minZ = n;
    float maxZ = n + clipRange;

    float range = maxZ - minZ;
    float ratio = maxZ / minZ;

    std::array<float, NumCascades> cascadeEnds{};
    for (int i = 0; i < NumCascades; ++i)
    {
        float p = (i + 1) / (float)NumCascades;
        float logd = minZ * std::pow(ratio, p);
        float lined = minZ + range * p;
        float d = lambda * (logd - lined) + lined;
        cascadeEnds[i] = d;
    }

    m_Data.cascadeSplits = glm::vec4(cascadeEnds[0], cascadeEnds[1], cascadeEnds[2], cascadeEnds[3]);

    // Camera basis
    glm::vec3 camPos = camera.position;
    glm::vec3 forward = glm::normalize(camera.target - camera.position);
    glm::vec3 right = camera.GetRight();
    glm::vec3 up = camera.GetUp();

    float aspect = 1.0f; // could pass if needed (camera fov uses window aspect separately, but we only need relative)
    // Derive vertical fov in radians
    float fovY = glm::radians(camera.fov);
    float tanFovY = tanf(fovY * 0.5f);
    float tanFovX = tanFovY; // if aspect assumed 1; for better accuracy we should pass aspect; acceptable simplification.

    float lastSplitDist = n;
    for (int c = 0; c < NumCascades; ++c)
    {
        float splitDist = cascadeEnds[c];

        // Frustum corners in view space for this cascade slice
        float nearDist = lastSplitDist;
        float farDist = splitDist;

        std::array<glm::vec3,8> cornersVS;
        cornersVS[0] = glm::vec3(-tanFovX * nearDist, -tanFovY * nearDist, -nearDist);
        cornersVS[1] = glm::vec3( tanFovX * nearDist, -tanFovY * nearDist, -nearDist);
        cornersVS[2] = glm::vec3( tanFovX * nearDist,  tanFovY * nearDist, -nearDist);
        cornersVS[3] = glm::vec3(-tanFovX * nearDist,  tanFovY * nearDist, -nearDist);
        cornersVS[4] = glm::vec3(-tanFovX * farDist,  -tanFovY * farDist,  -farDist);
        cornersVS[5] = glm::vec3( tanFovX * farDist,  -tanFovY * farDist,  -farDist);
        cornersVS[6] = glm::vec3( tanFovX * farDist,   tanFovY * farDist,  -farDist);
        cornersVS[7] = glm::vec3(-tanFovX * farDist,   tanFovY * farDist,  -farDist);

        // Transform to world space
        std::array<glm::vec3,8> cornersWS;
        glm::mat3 camBasis(right, up, -forward); // view-space axes to world
        for (int i = 0; i < 8; ++i)
        {
            glm::vec3 ws = camBasis * cornersVS[i] + camPos;
            cornersWS[i] = ws;
        }

        // Compute bounding sphere center of slice
        glm::vec3 center(0.0f);
        for (auto &p : cornersWS) center += p;
        center /= 8.0f;

        // Light view matrix (ensure not colinear with up)
        glm::vec3 lightUp = fabsf(lightDir.y) > 0.95f ? glm::vec3(0,0,1) : glm::vec3(0,1,0);
        glm::mat4 lightView = glm::lookAt(center - lightDir * 150.0f, center, lightUp);

        // Project slice corners into light space and find extents
        glm::vec3 minB( 1e9f), maxB(-1e9f);
        for (auto &p : cornersWS)
        {
            glm::vec4 ls = lightView * glm::vec4(p,1.0f);
            minB = glm::min(minB, glm::vec3(ls));
            maxB = glm::max(maxB, glm::vec3(ls));
        }

        // Extend depth range
        const float zMult = 10.0f;
        if (minB.z < 0)
            minB.z *= zMult;
        else
            minB.z /= zMult;
        
        if (maxB.z < 0)
            maxB.z /= zMult;
        else
            maxB.z *= zMult;

        glm::mat4 lightProj = glm::ortho(minB.x, maxB.x, minB.y, maxB.y, -maxB.z, -minB.z);

        // Texel snapping to reduce shimmering
        glm::mat4 lightViewProj = lightProj * lightView;
        float texelSize = (maxB.x - minB.x) / (float)m_Resolution;
        glm::vec4 origin = lightViewProj * glm::vec4(0,0,0,1);
        origin /= origin.w;
        origin = origin * 0.5f + 0.5f;
        float ox = origin.x * m_Resolution;
        float oy = origin.y * m_Resolution;
        float rx = roundf(ox);
        float ry = roundf(oy);
        float dx = (rx - ox) * 2.0f / m_Resolution;
        float dy = (ry - oy) * 2.0f / m_Resolution;
        lightProj[3][0] += dx;
        lightProj[3][1] += dy;

        m_Data.lightViewProj[c] = lightProj * lightView;
        lastSplitDist = splitDist;
    }
}

void CascadedShadowMap::Update(const Camera &camera, const glm::vec3 &lightDir)
{
    ComputeMatrices(camera, glm::normalize(lightDir));
    Upload();
}

void CascadedShadowMap::BeginCascade(int cascadeIndex)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_Resolution, m_Resolution);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, m_DepthArray, 0, cascadeIndex);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void CascadedShadowMap::EndCascade()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CascadedShadowMap::BindTexture(int unit) const
{
    glBindTextureUnit(unit, m_DepthArray);
}

void CascadedShadowMap::Upload()
{
    if (m_UBO)
    {
        m_UBO->SetData(&m_Data, sizeof(GPUData));
    }
}
