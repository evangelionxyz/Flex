// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef COMPONENTS_H
#define COMPONENTS_H

#include <string>
#include <glm/glm.hpp>

#include "Core/UUID.h"

namespace JPH
{
    class Body;
}

namespace flex
{
    struct TagComponent
    {
        std::string name;
        UUID uuid;

        TagComponent(const std::string& name, const UUID& uuid)
            : name(name), uuid(uuid)
        {
        }

        TagComponent() = default;
    };

	struct Rigidbody
	{
        enum class EMotionQuality
        {
            Discrete = 0,
            LinearCast = 1
        };

        EMotionQuality MotionQuality = EMotionQuality::Discrete;

        bool useGravity = true;
        bool rotateX = true, rotateY = true, rotateZ = true;
        bool moveX = true, moveY = true, moveZ = true;
        bool isStatic = false;
        float mass = 1.0f;
        bool allowSleeping = true;
        bool retainAcceleration = false;
        float gravityFactor = 1.0f;
        glm::vec3 centerMass = { 0.0f, 0.0f, 0.0f };

        JPH::Body* body = nullptr;

        Rigidbody() = default;
	};

	struct PhysicsCollider
	{
		float friction = 0.6f;
		float staticFriction = 0.6f;
		float restitution = 0.6f;
		float density = 1.0f;
		void* shape = nullptr;
	};

	struct BoxCollider : public PhysicsCollider
	{
		glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
		BoxCollider() = default;
	};
}

#endif