// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "Camera.h"
#include "Renderer/Window.h"

namespace flex
{
    void Camera::UpdateMouseState()
    {
        auto window = Window::Get();
        mouse.lastPosition = mouse.position;
        mouse.position = window->GetMousePosition();
        
        mouse.leftButton = window->IsMouseButtonPressed(SDL_BUTTON_LEFT);
        mouse.middleButton = window->IsMouseButtonPressed(SDL_BUTTON_MIDDLE);
        mouse.rightButton = window->IsMouseButtonPressed(SDL_BUTTON_RIGHT);
    }

    void Camera::HandleOrbit(float deltaTime)
    {
        auto window = Window::Get();

        if (mouse.leftButton)
        {
            auto delta = mouse.position - mouse.lastPosition;
            // Handle Zoom
            if (window->IsKeyModPressed(SDL_KMOD_LCTRL))
            {
                if (delta.y != 0.0f)
                {
                    if (controls.enableInertia)
                    {
                        zoomVelocity += delta.y;
                    }
                    else
                    {
                        distance -= delta.y;
                        distance = glm::clamp(distance, controls.minDistance, controls.maxDistance);
                    }
                }
                
                // Apply zoom velocity
                if (controls.enableInertia && std::abs(zoomVelocity) > 0.001f)
                {
                    distance -= zoomVelocity * deltaTime;
                    distance = glm::clamp(distance, controls.minDistance, controls.maxDistance);
                    
                    // Dampen zoom velocity
                    zoomVelocity *= controls.zoomDamping;
                    
                    // Stop very small velocities
                    if (std::abs(zoomVelocity) < 0.001f)
                    {
                        zoomVelocity = 0.0f;
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

    void Camera::HandlePan()
    {
        if (mouse.middleButton)
        {
            auto delta = mouse.position - mouse.lastPosition;
            
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

    void Camera::HandleZoom(float deltaTime)
    {
        auto window = Window::Get();

        // Handle mouse wheel
        float wheelDelta = 0.0f;
        
        // Check for scroll wheel input
        if (mouse.scroll.y != 0)
        {
            wheelDelta = mouse.scroll.y;
            // Reset scroll after processing
            mouse.scroll.y = 0;
        }
        
        // Handle keyboard zoom controls
        if (wheelDelta != 0.0f)
        {
            // Apply zoom velocity for smooth zooming
            if (controls.enableInertia)
            {
                zoomVelocity += wheelDelta * controls.zoomSensitivity;
            }
            else
            {
                // Direct zoom for immediate response
                distance -= wheelDelta * controls.zoomSensitivity;
                distance = glm::clamp(distance, controls.minDistance, controls.maxDistance);
            }
        }
        
        // Apply zoom velocity
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
    }

    void Camera::ApplyInertia(float deltaTime)
    {
        // Apply angular inertia
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
        
        // Apply pan inertia
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
    }

    void Camera::UpdateCameraPosition()
    {
        UpdateSphericalPosition();
        view = glm::lookAt(position, target, up);
    }
}