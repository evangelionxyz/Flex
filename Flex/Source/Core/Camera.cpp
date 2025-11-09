// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "Camera.h"
#include "Renderer/Window.h"

namespace flex
{
    void Camera::HandleOrbit(const glm::vec2 &delta)
    {
        auto window = Window::Get();

        if (window->IsMouseButtonPressed(SDL_BUTTON_RIGHT))
        {
            // Handle Zoom
            if (window->IsKeyModPressed(SDL_KMOD_LCTRL))
            {
                if (delta.y != 0.0f)
                {
                    if (controls.enableInertia)
                    {
                        zoomVelocity += delta.y * 0.5f;
                    }
                    else
                    {
                        distance -= delta.y * 0.5f;
                        distance = glm::clamp(distance, controls.minDistance, controls.maxDistance);
                    }
                }
            }
            // Handle Orbit
            else
            {
                // Apply mouse movement to angular velocity for inertia
                if (controls.enableInertia)
                {
                    angularVelocity.x += delta.x * controls.mouseSensitivity;
                    angularVelocity.y += delta.y * controls.mouseSensitivity;
                }
                
                // Directly update angles for immediate response
                yaw += delta.x * controls.mouseSensitivity;
                pitch += delta.y * controls.mouseSensitivity;
                
                // Clamp pitch to prevent camera flipping
                pitch = glm::clamp(pitch, controls.minPitch, controls.maxPitch);
            }
        }
    }

    void Camera::HandlePan(const glm::vec2 &delta)
    {
        auto window = Window::Get();

        if (window->IsMouseButtonPressed(SDL_BUTTON_MIDDLE))
        {
            // Calculate pan direction in camera space
            auto right = GetRight();
            auto worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
            
            // Pan in the camera's right and world up directions
            const float panSpeed = controls.panSensitivity * distance;
            auto panVector = right * (-delta.x * panSpeed) + worldUp * (delta.y * panSpeed);
            
            // Apply pan to target
            target += panVector;
            
            // Apply to pan velocity for inertia
            if (controls.enableInertia)
            {
                panVelocity = delta * controls.panSensitivity;
            }
        }
    }

    void Camera::HandleZoom(const float &yoffset)
    {
        // Handle keyboard zoom controls
        if (yoffset != 0.0f)
        {
            if (controls.enableInertia)
            {
                zoomVelocity += yoffset * controls.zoomSensitivity;
            }
            else
            {
                distance -= yoffset * controls.zoomSensitivity;
                distance = glm::clamp(distance, controls.minDistance, controls.maxDistance);
            }
        }
    }

    void Camera::OnUpdate(float deltaTime)
    {
        // Apply zoom velocity (Zooming)
        if (controls.enableInertia && std::abs(zoomVelocity) > 0.001f)
        {
            distance -= zoomVelocity * deltaTime * 10.0f;
            distance = glm::clamp(distance, controls.minDistance, controls.maxDistance);

            // Dampen zoom velocity
            zoomVelocity *= controls.zoomDamping;

            // Stop very small velocities
            if (std::abs(zoomVelocity) < 0.001f)
            {
                zoomVelocity = 0.0f;
            }
        }

        // Apply angular inertia (Rotation)
        if (glm::length(angularVelocity) > 0.001f)
        {
            yaw += angularVelocity.x * deltaTime;
            pitch += angularVelocity.y * deltaTime;
            pitch = glm::clamp(pitch, controls.minPitch, controls.maxPitch);

            // Dampen angular velocity
            angularVelocity *= controls.inertiaDamping;

            // Stop very small velocities
            if (glm::length(angularVelocity) < 0.001f)
            {
                angularVelocity = glm::vec2(0.0f);
            }
        }

        // Apply pan inertia (Panning)
        if (glm::length(panVelocity) > 0.001f)
        {
            glm::vec3 right = GetRight();
            glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

            const float panSpeed = distance;
            glm::vec3 panVector = right * (-panVelocity.x * panSpeed) + worldUp * (panVelocity.y * panSpeed);

            target += panVector * deltaTime;

            // Dampen pan velocity
            panVelocity *= controls.inertiaDamping;

            // Stop very small velocities
            if (glm::length(panVelocity) < 0.001f)
            {
                panVelocity = glm::vec2(0.0f);
            }
        }

        UpdateSphericalPosition();

        view = glm::lookAt(position, target, up);
    }
    void Camera::UpdateSphericalPosition()
    {
        position.x = target.x + distance * cos(pitch) * cos(yaw);
        position.y = target.y + distance * sin(pitch);
        position.z = target.z + distance * cos(pitch) * sin(yaw);
    }
}