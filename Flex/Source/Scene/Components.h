// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef COMPONENTS_H
#define COMPONENTS_H

#include <algorithm>
#include <set>
#include <string>
#include <glm/glm.hpp>

#include "Scene.h"
#include "Renderer/Mesh.h"
#include "Core/Types.h"
#include "Core/UUID.h"

namespace JPH
{
    class Body;
}

namespace flex
{
    class Scene;

    struct TagComponent
    {
        std::string name;
        Scene *scene = nullptr;

        UUID uuid;
        UUID parent = UUID(0);
        std::set<UUID> children;

        TagComponent(const std::string& name, const UUID& uuid)
            : name(name), uuid(uuid)
        {
        }

        TagComponent() = default;

        void AddChild(const UUID& childID)
        {
            entt::entity e = scene->GetEntityByUUID(childID);
            if (scene->IsValid(e))
            {
                auto& tag = scene->GetComponent<TagComponent>(e);
                tag.parent = uuid;
            }

            children.insert(childID);
        }

        void RemoveChild(const UUID& childID)
        {
            auto it = std::find(children.begin(), children.end(), childID);
            if (it != children.end())
            {
                entt::entity e = scene->GetEntityByUUID(childID);
                if (scene->IsValid(e))
                {
                    auto& tag = scene->GetComponent<TagComponent>(e);
                    tag.parent = UUID(0);
                }

                children.erase(it);
            }
        }
    };

    struct TransformComponent
    {
        glm::vec3 position = { 0.0f, 0.0f, 0.0f };
        glm::vec3 rotation = { 0.0f, 0.0f, 0.0f }; // Euler angles in degrees
        glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
        
        TransformComponent() = default;
    };

	struct RigidbodyComponent
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
        glm::vec3 centerOfMass = { 0.0f, 0.0f, 0.0f };

        JPH::Body* body = nullptr;

        RigidbodyComponent() = default;
	};

	struct IPhysicsColliderComponent
	{
		float friction = 0.6f;
		float staticFriction = 0.6f;
		float restitution = 0.6f;
		float density = 1.0f;
		void* shape = nullptr;
	};

	struct BoxColliderComponent : public IPhysicsColliderComponent
	{
		glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
		glm::vec3 offset = { 0.0f, 0.0f, 0.0f };

        BoxColliderComponent() = default;
	};

    struct MeshComponent
    {
        std::string meshPath;
        Ref<MeshInstance> meshInstance;
        
        MeshComponent() = default;
    };
}

#endif