// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef PHYSICS3D_H
#define PHYSICS3D_H

#include "Core/Types.h"
#include "entt/entt.hpp"

#include <Jolt/Jolt.h>

// Jolt includes
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>

// STL includes
#include <iostream>
#include <cstdarg>
#include <thread>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace flex
{
	struct RigidbodyComponent;
	struct TransformComponent;
	struct BoxColliderComponent;

	static JPH::Vec3 GlmToJoltVec3(const glm::vec3& v)
	{
		return JPH::Vec3(v.x, v.y, v.z);
	}

	static glm::vec3 JoltToGlmVec3(const JPH::Vec3& v)
	{
		return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
	}

	static JPH::Quat GlmToJoltQuat(const glm::quat& q)
	{
		return JPH::Quat(q.x, q.y, q.z, q.w);
	}

	static glm::quat JoltToGlmQuat(const JPH::Quat& q)
	{
		return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
	}

	namespace PhysicsLayers
	{
		static constexpr JPH::ObjectLayer NON_MOVING = 0;
		static constexpr JPH::ObjectLayer MOVING = 1;
		static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
	};

	/// Class that determines if two object layers can collide
	class JoltObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
	{
	public:
		virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
		{
			switch (inObject1)
			{
			case PhysicsLayers::NON_MOVING:
				return inObject2 == PhysicsLayers::MOVING; // Non moving only collides with moving
			case PhysicsLayers::MOVING:
				return true; // Moving collides with everything
			default:
				JPH_ASSERT(false);
				return false;
			}
		}
	};

	// Each broadphase layer results in a separate bounding volume tree in the broad phase. You at least want to have
	// a layer for non-moving and moving objects to avoid having to update a tree full of static objects every frame.
	// You can have a 1-on-1 mapping between object layers and broadphase layers (like in this case) but if you have
	// many object layers you'll be creating many broad phase trees, which is not efficient. If you want to fine tune
	// your broadphase layers define JPH_TRACK_BROADPHASE_STATS and look at the stats reported on the TTY.
	namespace BroadPhaseLayers
	{
		static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
		static constexpr JPH::BroadPhaseLayer MOVING(1);
		static constexpr JPH::uint NUM_LAYERS(2);
	};

	// BroadPhaseLayerInterface implementation
	// This defines a mapping between object and broadphase layers.
	class JoltBroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
	{
	public:
		JoltBroadPhaseLayerInterfaceImpl()
		{
			// Create a mapping table from object to broad phase layer
			mObjectToBroadPhase[PhysicsLayers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
			mObjectToBroadPhase[PhysicsLayers::MOVING] = BroadPhaseLayers::MOVING;
		}

		virtual JPH::uint GetNumBroadPhaseLayers() const override
		{
			return BroadPhaseLayers::NUM_LAYERS;
		}

		virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
		{
			JPH_ASSERT(inLayer < PhysicsLayers::NUM_LAYERS);
			return mObjectToBroadPhase[inLayer];
		}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
		virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
		{
			switch ((JPH::BroadPhaseLayer::Type)inLayer)
			{
			case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:	return "NON_MOVING";
			case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:		return "MOVING";
			default:													JPH_ASSERT(false); return "INVALID";
			}
		}
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

	private:
		JPH::BroadPhaseLayer mObjectToBroadPhase[PhysicsLayers::NUM_LAYERS];
	};

	/// Class that determines if an object layer can collide with a broadphase layer
	class JoltObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
	{
	public:
		virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
		{
			switch (inLayer1)
			{
			case PhysicsLayers::NON_MOVING:
				return inLayer2 == BroadPhaseLayers::MOVING;
			case PhysicsLayers::MOVING:
				return true;
			default:
				JPH_ASSERT(false);
				return false;
			}
		}
	};

	// An example contact listener
	class JoltContactListener : public JPH::ContactListener
	{
	public:
		// See: ContactListener
		virtual JPH::ValidateResult	OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult) override
		{
			std::cout << "Contact validate callback\n";

			// Allows you to ignore a contact before it is created (using layers to not make objects collide is cheaper!)
			return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
		}

		virtual void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override
		{
			std::cout << "A contact was added\n";
		}

		virtual void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override
		{
			std::cout << "A contact was persisted\n";
		}

		virtual void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override
		{
			std::cout << "A contact was removed\n";
		}
	};

	// An example activation listener
	class JoltBodyActivationListener : public JPH::BodyActivationListener
	{
	public:
		virtual void OnBodyActivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override
		{
			std::cout << "A body got activated\n";
		}

		virtual void OnBodyDeactivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override
		{
			std::cout << "A body went to sleep\n";
		}
	};

	class Scene;
	class JoltPhysics
	{
	public:
		static void Init();
		static void Shutdown();

		static JoltPhysics* Get();
		
		Scope<JPH::TempAllocator> tempAllocator;
		Scope<JPH::JobSystem> jobSystem;
		Scope<JPH::BodyActivationListener> bodyActivationListener;
		Scope<JPH::ContactListener> contactListener;

		JoltBroadPhaseLayerInterfaceImpl broadPhaseLayer;
		JoltObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter;
		JoltObjectLayerPairFilterImpl objectLayerPairFilter;
	};

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