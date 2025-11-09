// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef MATH_HPP
#define MATH_HPP

#ifndef GLM_ENABLE_EXPERIMENTAL
    #define GLM_ENABLE_EXPERIMENTAL
#endif

#include <glm/glm.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Scene/Components.h"

namespace flex::math
{
    static glm::mat4 ComposeTransform(const TransformComponent& transform)
	{
		const glm::mat4 translation = glm::translate(glm::mat4(1.0f), transform.position);
		const glm::quat orientation = glm::quat(glm::radians(transform.rotation));
		const glm::mat4 rotation = glm::toMat4(orientation);
		const glm::mat4 scale = glm::scale(glm::mat4(1.0f), transform.scale);
		return translation * rotation * scale;
	}

	static void DecomposeTransform(const glm::mat4& matrix, TransformComponent& outTransform)
	{
		glm::vec3 scale;
		glm::vec3 skew;
		glm::vec4 perspective;
		glm::quat orientation;
		glm::vec3 translation;

		if (!glm::decompose(matrix, scale, orientation, translation, skew, perspective))
		{
			translation = glm::vec3(0.0f);
			orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			scale = glm::vec3(1.0f);
		}

		orientation = glm::normalize(orientation);

		outTransform.position = translation;
		outTransform.scale = scale;
		outTransform.rotation = glm::degrees(glm::eulerAngles(orientation));
	}
}

#endif