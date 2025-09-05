#pragma once

#include "Math/Math.hpp"

struct MouseState
{
	glm::vec2 position;
	glm::vec2 lastPosition;
	glm::ivec2 scroll {0.0, 0.0};
	bool leftButton = false;
	bool middleButton = false;
	bool rightButton = false;
};

struct CameraBuffer
{
	glm::mat4 viewProjection;
	glm::vec4 position;
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
	float fov = 45.0f;
	float nearPlane = 0.1f;
	float farPlane = 550.0f;
	
	// Mouse state
	MouseState mouse;
	
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
	} controls;
	
	// Inertia variables
	glm::vec2 angularVelocity = glm::vec2(0.0f);
	glm::vec2 panVelocity = glm::vec2(0.0f);
	float zoomVelocity = 0.0f;
	
	// Matrices
	glm::mat4 view;
	glm::mat4 projection;
	
	// Update camera with new position based on spherical coordinates
	void UpdateSphericalPosition()
	{
		position.x = target.x + distance * cos(pitch) * cos(yaw);
		position.y = target.y + distance * sin(pitch);
		position.z = target.z + distance * cos(pitch) * sin(yaw);
	}
	
	// Update view and projection matrices
	void UpdateMatrices(float aspectRatio)
	{
		view = glm::lookAt(position, target, up);
		projection = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
	}
	
	// Get camera direction vectors
	glm::vec3 GetForward() const { return glm::normalize(target - position); }
	glm::vec3 GetRight() const { return glm::normalize(glm::cross(GetForward(), up)); }
	glm::vec3 GetUp() const { return glm::normalize(glm::cross(GetRight(), GetForward())); }
};

// Legacy functions for backward compatibility
inline glm::vec3 GetCameraRight(const Camera &camera) {
	return camera.GetRight();
}

inline glm::vec3 GetCameraForward(const Camera &camera) {
	return camera.GetForward();
}

inline glm::vec3 GetCameraUp(const Camera &camera) {
	return camera.GetUp();
}
