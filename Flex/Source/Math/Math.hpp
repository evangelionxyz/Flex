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

namespace flex
{
    struct Vector2f
    {
        float x, y;
        
        Vector2f(): x(0.0f), y(0.0f) {}
        Vector2f(float x, float y) : x(x), y(y) {}
    };

    struct Vector3f
    {
        float x, y, z;

        Vector3f(): x(0.0f), y(0.0f), z(0.0f) {}
        Vector3f(float x, float y, float z) : x(x), y(y), z(z) {}
    };
}

#endif