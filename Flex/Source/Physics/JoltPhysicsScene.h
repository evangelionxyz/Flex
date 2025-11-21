// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef JOLT_PHYSICS_SCENE_H
#define JOLT_PHYSICS_SCENE_H

#include "JoltPhysics.h"
#include "Core/Types.h"

namespace flex
{
    struct RigidbodyComponent;
	struct TransformComponent;
	struct BoxColliderComponent;

    class Scene;

    class JoltPhysicsScene
	{
	public:
		JoltPhysicsScene(Scene* scene);
		~JoltPhysicsScene();

		static Ref<JoltPhysicsScene> Create(Scene *scene);

		void InstantiateEntity(entt::entity entity);
		void DestroyEntity(entt::entity entity);

		void SimulationStart();
		void SimulationStop();
		void Simulate(float deltaTime);

		JPH::BodyCreationSettings CreateBody(JPH::ShapeRefC shape, RigidbodyComponent &rb, const glm::vec3 &position, const glm::quat &rotation);

		void CreateBoxCollider(entt::entity entity);
		void CreateSphereCollider(entt::entity entity);

		void AddForce(JPH::BodyID bodyID, const glm::vec3& force);
		void AddTorque(JPH::BodyID bodyID, const glm::vec3& torque);
		void AddForceAndTorque(JPH::BodyID bodyID, const glm::vec3& force, const glm::vec3& torque);
		void AddAngularImpulse(JPH::BodyID bodyID, const glm::vec3& impulse);
		void ActivateBody(JPH::BodyID bodyID);
		void DeactivateBody(JPH::BodyID bodyID);
		void DestroyBody(JPH::BodyID bodyID);
		bool IsActive(JPH::BodyID bodyID);
		void MoveKinematic(JPH::BodyID bodyID, const glm::vec3& targetPosition, const glm::vec3& targetRotation, float deltaTime);
		void AddImpulse(JPH::BodyID bodyID, const glm::vec3& impulse);
		void AddLinearVelocity(JPH::BodyID bodyID, const glm::vec3& velocity);
		void SetPosition(JPH::BodyID bodyID, const glm::vec3& position, bool activate);
		void SetEulerAngleRotation(JPH::BodyID bodyID, const glm::vec3& rotation, bool activate);
		void SetRotation(JPH::BodyID bodyID, const glm::quat& rotation, bool activate);
		void SetLinearVelocity(JPH::BodyID bodyID, const glm::vec3& vel);
		void SetFriction(JPH::BodyID bodyID, float value);
		void SetRestitution(JPH::BodyID bodyID, float value);
		void SetGravityFactor(JPH::BodyID bodyID, float value);
		float GetRestitution(JPH::BodyID bodyID);
		float GetFriction(JPH::BodyID bodyID);
		float GetGravityFactor(JPH::BodyID bodyID);
		glm::vec3 GetPosition(JPH::BodyID bodyID);
		glm::vec3 GetEulerAngles(JPH::BodyID bodyID);
		glm::quat GetRotation(JPH::BodyID bodyID);
		glm::vec3 GetCenterOfMassPosition(JPH::BodyID bodyID);
		glm::vec3 GetLinearVelocity(JPH::BodyID bodyID);
		void SetMaxLinearVelocity(JPH::BodyID bodyID, float max);
		void SetMaxAngularVelocity(JPH::BodyID bodyID, float max);

		JPH::BodyInterface* GetBodyInterface() { return m_BodyInterface; }
	private:
		Scene* m_Scene;
		JPH::BodyInterface* m_BodyInterface;
		JPH::PhysicsSystem m_PhysicsSystem;
	};
}

#endif