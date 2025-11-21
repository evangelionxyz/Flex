// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef COMPONENTS_H
#define COMPONENTS_H

#include <algorithm>
#include <functional>
#include <set>
#include <string>

#include <glm/gtc/matrix_transform.hpp>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

#include "Scene.h"
#include "Renderer/Mesh.h"
#include "Core/Types.h"
#include "Core/UUID.h"
#include "Core/Camera.h"

namespace flex
{
    class Scene;
    class ScriptableEntity;
    struct PhysicsContactData;
    struct PhysicsActivationData;

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

    using ContactValidationCallback = std::function<JPH::ValidateResult(const PhysicsContactData&)>;
    using ContactCallback = std::function<void(const PhysicsContactData&)>;
    using ActivationCallback = std::function<void(const PhysicsActivationData&)>;

    enum class CameraAspectMode
    {
        Free = 0,
        Fixed = 1
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
        float friction = 0.6f;
		float staticFriction = 0.6f;
		float restitution = 0.6f;
        bool allowSleeping = true;
        bool retainAcceleration = false;
        float gravityFactor = 1.0f;
        glm::vec3 centerOfMass = { 0.0f, 0.0f, 0.0f };

        JPH::BodyID bodyID = JPH::BodyID();

        ContactValidationCallback onContactValidate;
        ContactCallback onContactEnter;
        ContactCallback onContactPersist;
        ContactCallback onContactExit;

        ActivationCallback onBodyActivated;
        ActivationCallback onBodyDeactivated;

        RigidbodyComponent() = default;
	};

	struct IPhysicsColliderComponent
	{
		glm::vec3 offset = { 0.0f, 0.0f, 0.0f };
        float density = 1.0f;
		void* shape = nullptr;
	};

	struct BoxColliderComponent : public IPhysicsColliderComponent
	{
		glm::vec3 scale = { 1.0f, 1.0f, 1.0f };
        BoxColliderComponent() = default;
	};

    struct CapsuleColliderComponent : public IPhysicsColliderComponent
    {
        float radius = 0.5f;
        float height = 1.0f;
        CapsuleColliderComponent() = default;
    };

    struct SphereColliderComponent : public IPhysicsColliderComponent
    {
        float radius = 0.5f;
        SphereColliderComponent() = default;
    };

    struct PlaneColliderComponent : public IPhysicsColliderComponent
    {
        PlaneColliderComponent() = default;
    };

    struct MeshComponent
    {
        std::string meshPath;
        Ref<MeshInstance> meshInstance;
        int meshIndex = -1;
        
        MeshComponent() = default;
    };

    struct CameraComponent
    {
        float fov = 45.0f;
        float nearPlane = 0.1f;
        float farPlane = 550.0f;
        float orthoSize = 10.0f;

        bool primary = false;

        glm::mat4 projection = glm::mat4(1.0f);
        glm::mat4 view = glm::mat4(1.0f);

        ProjectionType projectionType = ProjectionType::Perspective;
        CameraAspectMode aspectMode = CameraAspectMode::Free;
        float fixedAspectRatio = 16.0f / 9.0f;
        float aspectRatio = 16.0f / 9.0f;
        glm::vec2 viewportSize = { 1280.0f, 720.0f };
        CameraLens lens;
        PostProcessing postProcessing;

        CameraComponent() = default;

        void RecalculateProjection(const glm::vec2& viewport)
        {
            viewportSize = viewport;

            const float safeWidth = viewportSize.x <= 0.0f ? 1.0f : viewportSize.x;
            const float safeHeight = viewportSize.y <= 0.0f ? 1.0f : viewportSize.y;
            const float freeAspect = safeWidth / safeHeight;

            if (aspectMode == CameraAspectMode::Fixed)
            {
                aspectRatio = fixedAspectRatio > 0.0f ? fixedAspectRatio : freeAspect;
            }
            else
            {
                aspectRatio = freeAspect;
            }

            if (projectionType == ProjectionType::Perspective)
            {
                projection = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
            }
            else
            {
                const float halfHeight = orthoSize * 0.5f;
                const float halfWidth = halfHeight * aspectRatio;
                projection = glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
            }
        }
    };

    struct NativeScriptComponent
    {
        ScriptableEntity *instance;
        ScriptableEntity *(*InstantiateScript)(Scene *scene, entt::entity entity);

        void (*DestroyScript)(NativeScriptComponent *nsc);

        template<typename T>
        void Bind()
        {
            InstantiateScript = [](Scene *scene, entt::entity entity)
            {
                return static_cast<ScriptableEntity *>(new T(scene, entity));
            };

            DestroyScript = [](NativeScriptComponent *nsc)
            {
                delete nsc->instance;
                nsc->instance = nullptr;
            };
        }

        NativeScriptComponent() = default;
    };
}

#endif