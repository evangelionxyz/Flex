#include "Camera.h"
#include <GLFW/glfw3.h>

void UpdateCamera(Camera &camera, GLFWwindow *window, float deltaTime, int windowWidth, int windowHeight)
{
	// Update mouse state
	UpdateMouseState(camera, window);
	
	// Handle input
	HandleOrbit(camera, deltaTime);
	HandlePan(camera, deltaTime);
	HandleZoom(camera, deltaTime, window);
	
	// Apply inertia if enabled
	if (camera.controls.enableInertia) {
		ApplyInertia(camera, deltaTime);
	}
	
	// Update camera position and matrices
	UpdateCameraPosition(camera);
	
	float aspect = (float)windowWidth / (float)windowHeight;
	camera.UpdateMatrices(aspect);
}

void UpdateMouseState(Camera &camera, GLFWwindow *window)
{
	// Get current mouse position
	double mouseX, mouseY;
	glfwGetCursorPos(window, &mouseX, &mouseY);
	
	// Store last position before updating
	camera.mouse.lastPosition = camera.mouse.position;
	
	// Update current position
	camera.mouse.position = glm::vec2((float)mouseX, (float)mouseY);
	
	// Update button states
	camera.mouse.leftButton = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
	camera.mouse.middleButton = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
	camera.mouse.rightButton = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
}

void HandleOrbit(Camera &camera, float deltaTime)
{
	if (camera.mouse.leftButton) {
		glm::vec2 delta = camera.mouse.position - camera.mouse.lastPosition;
		
		// Apply mouse movement to angular velocity for inertia
		if (camera.controls.enableInertia) {
			camera.angularVelocity.x += delta.x * camera.controls.mouseSensitivity;
			camera.angularVelocity.y += delta.y * camera.controls.mouseSensitivity;
		}
		
		// Directly update angles for immediate response
		camera.yaw += delta.x * camera.controls.mouseSensitivity;
		camera.pitch += delta.y * camera.controls.mouseSensitivity;
		
		// Clamp pitch to prevent camera flipping
		camera.pitch = glm::clamp(camera.pitch, camera.controls.minPitch, camera.controls.maxPitch);
	}
}

void HandlePan(Camera &camera, float deltaTime)
{
	if (camera.mouse.middleButton) {
		glm::vec2 delta = camera.mouse.position - camera.mouse.lastPosition;
		
		// Calculate pan direction in camera space
		glm::vec3 right = camera.GetRight();
		glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
		
		// Pan in the camera's right and world up directions
		float panSpeed = camera.controls.panSensitivity * camera.distance;
		glm::vec3 panVector = right * (-delta.x * panSpeed) + 
							 worldUp * (delta.y * panSpeed);
		
		// Apply pan to target
		camera.target += panVector;
		
		// Apply to pan velocity for inertia
		if (camera.controls.enableInertia) {
			camera.panVelocity = delta * camera.controls.panSensitivity;
		}
	}
}

void HandleZoom(Camera &camera, float deltaTime, GLFWwindow *window)
{
	// Handle mouse wheel
	float wheelDelta = 0.0f;
	
	// Check for scroll wheel input
	if (camera.mouse.scroll.y != 0) {
		wheelDelta = camera.mouse.scroll.y;
		// Reset scroll after processing
		camera.mouse.scroll.y = 0;
	}
	
	// Handle keyboard zoom controls
	if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS || 
		glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS) {
		wheelDelta -= camera.controls.zoomSensitivity * deltaTime * 10.0f;
	}
	if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS || 
		glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) {
		wheelDelta += camera.controls.zoomSensitivity * deltaTime * 10.0f;
	}
	
	if (wheelDelta != 0.0f) {
		// Apply zoom velocity for smooth zooming
		if (camera.controls.enableInertia) {
			camera.zoomVelocity += wheelDelta * camera.controls.zoomSensitivity;
		} else {
			// Direct zoom for immediate response
			camera.distance -= wheelDelta * camera.controls.zoomSensitivity;
			camera.distance = glm::clamp(camera.distance, camera.controls.minDistance, camera.controls.maxDistance);
		}
	}
	
	// Apply zoom velocity
	if (camera.controls.enableInertia && abs(camera.zoomVelocity) > 0.001f) {
		camera.distance -= camera.zoomVelocity * deltaTime * 10.0f;
		camera.distance = glm::clamp(camera.distance, camera.controls.minDistance, camera.controls.maxDistance);
		
		// Dampen zoom velocity
		camera.zoomVelocity *= camera.controls.zoomDamping;
		
		// Stop very small velocities
		if (abs(camera.zoomVelocity) < 0.001f) {
			camera.zoomVelocity = 0.0f;
		}
	}
}

void ApplyInertia(Camera &camera, float deltaTime)
{
	// Apply angular inertia
	if (glm::length(camera.angularVelocity) > 0.001f) {
		camera.yaw += camera.angularVelocity.x * deltaTime;
		camera.pitch += camera.angularVelocity.y * deltaTime;
		camera.pitch = glm::clamp(camera.pitch, camera.controls.minPitch, camera.controls.maxPitch);
		
		// Dampen angular velocity
		camera.angularVelocity *= camera.controls.inertiaDamping;
		
		// Stop very small velocities
		if (glm::length(camera.angularVelocity) < 0.001f) {
			camera.angularVelocity = glm::vec2(0.0f);
		}
	}
	
	// Apply pan inertia
	if (glm::length(camera.panVelocity) > 0.001f)
	{
		glm::vec3 right = camera.GetRight();
		glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
		
		float panSpeed = camera.distance;
		glm::vec3 panVector = right * (-camera.panVelocity.x * panSpeed) + worldUp * (camera.panVelocity.y * panSpeed);
		
		camera.target += panVector * deltaTime;
		
		// Dampen pan velocity
		camera.panVelocity *= camera.controls.inertiaDamping;
		
		// Stop very small velocities
		if (glm::length(camera.panVelocity) < 0.001f) {
			camera.panVelocity = glm::vec2(0.0f);
		}
	}
}

void UpdateCameraPosition(Camera &camera)
{
	// Update camera position based on spherical coordinates
	camera.UpdateSphericalPosition();
	
	// Update view matrix
	camera.view = glm::lookAt(camera.position, camera.target, camera.up);
}
