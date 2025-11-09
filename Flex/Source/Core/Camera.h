// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef CAMERA_H
#define CAMERA_H

#include "Math/Math.hpp"

namespace flex
{
    struct CameraBuffer
    {
        glm::mat4 viewProjection;
        glm::mat4 view;
        glm::vec4 position;
    };

    struct PostProcessing
    {
        // Toggles
        bool enableVignette = true;
        bool enableChromAb = false;
        bool enableBloom = true;
        bool enableSSAO = false; // Screen space ambient occlusion
        bool debugSSAO = false; // Visualize raw AO buffer
        
        // Vignette params
        float vignetteRadius = 1.1f;
        float vignetteSoftness = 0.7f;
        float vignetteIntensity = 0.8f;
        glm::vec3 vignetteColor = glm::vec3(0.0f);
        
        // Chromatic aberration params
        float chromAbAmount = 0.001f;
        float chromAbRadial = 0.1f;

        // SSAO params
        float aoRadius = 0.5f;
        float aoBias = 0.025f;
        float aoIntensity = 1.0f; // blend strength when applied in post
        float aoPower = 1.0f;     // curve/power for contrast
    };

    enum class ProjectionType
    {
        Perspective,
        Orthographic,
    };

    struct CameraLens
    {
        float focalLength = 120.0f;
        float focalDistance = 5.5f;
        float fStop = 1.4f;
        float focusRange = 5.0f;
        float blurAmount = 1.0f;
        float exposure = 1.1f;
        float gamma = 1.1f;
        bool enableDOF = true;
    };

    struct Camera
    {
        // Core camera properties
        glm::vec3 position = glm::vec3(0.0f, 0.0f, 5.0f);
        glm::vec3 target = glm::vec3(0.0f);
        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        
        // Spherical coordinates
        float yaw = 0.0f;
        float pitch = 0.0f;
        float distance = 1.0f;
        
        // Projection properties
        float fov = 90.0f;
        float nearPlane = 0.1f;
        float farPlane = 550.0f;
        
        CameraLens lens;
        PostProcessing postProcessing;
        ProjectionType projectionType = ProjectionType::Perspective;
        float orthoSize = 10.0f; // Half-height for orthographic projection
        
        // Control settings
        struct Controls
        {
            float mouseSensitivity = 0.003f;
            float zoomSensitivity = 2.0f;
            float panSensitivity = 0.001f;
            float minDistance = 0.5f;
            float maxDistance = 50.0f;
            float minPitch = -glm::radians(89.0f);
            float maxPitch = glm::radians(89.0f);
            bool enableInertia = true;
            float inertiaDamping = 0.9f;
            float zoomDamping = 0.65f;

            int debugShadowMode = 0; // 0 off, 1 cascade index, 2 visibilities
        };
        
        Controls controls;
        
        // Inertia variables
        glm::vec2 angularVelocity = glm::vec2(0.0f);
        glm::vec2 panVelocity = glm::vec2(0.0f);
        float zoomVelocity = 0.0f;
        
        // Matrices
        glm::mat4 view;
        glm::mat4 projection;
        
        // Get camera direction vectors
        glm::vec3 GetForward() const { return glm::normalize(target - position); }
        glm::vec3 GetRight() const { return glm::normalize(glm::cross(GetForward(), up)); }
        glm::vec3 GetUp() const { return glm::normalize(glm::cross(GetRight(), GetForward())); }

        void HandleOrbit(const glm::vec2 &delta);
        void HandlePan(const glm::vec2 &delta);
        void HandleZoom(const float &offset);

        void OnUpdate(float deltaTime);

        // Update view and projection matrices
        void UpdateMatrices(float aspectRatio)
        {
            view = glm::lookAt(position, target, up);
            if (projectionType == ProjectionType::Perspective)
            {
                projection = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
            }
            else // Orthographic
            {
                float halfH = orthoSize * 0.5f;
                float halfW = halfH * aspectRatio;
                projection = glm::ortho(-halfW, halfW, -halfH, halfH, nearPlane, farPlane);
            }
        }

    private:
        void UpdateSphericalPosition();
    };

}

#endif