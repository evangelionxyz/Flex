// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef JOLT_LISTENERS_H
#define JOLT_LISTENERS_H

#include "Core/Types.h"

#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyID.h>

#include "entt/entt.hpp"

#include <functional>
#include <shared_mutex>
#include <unordered_map>

namespace flex
{
	class Scene;
	struct RigidbodyComponent;

	enum class PhysicsContactPhase
	{
		Validate,
		Enter,
		Persist,
		Exit
	};

	struct PhysicsContactData
	{
		Scene* scene = nullptr;
		PhysicsContactPhase phase = PhysicsContactPhase::Enter;
		entt::entity selfEntity = entt::null;
		entt::entity otherEntity = entt::null;
		RigidbodyComponent* selfRigidbody = nullptr;
		RigidbodyComponent* otherRigidbody = nullptr;
		const JPH::Body* selfBody = nullptr;
		const JPH::Body* otherBody = nullptr;
		const JPH::ContactManifold* manifold = nullptr;
		JPH::ContactSettings* settings = nullptr;
		const JPH::CollideShapeResult* collisionResult = nullptr;
		const JPH::SubShapeIDPair* subShapePair = nullptr;
		JPH::RVec3 baseOffset = JPH::RVec3::sZero();
	};

	struct PhysicsActivationData
	{
		Scene* scene = nullptr;
		entt::entity entity = entt::null;
		RigidbodyComponent* rigidbody = nullptr;
		JPH::BodyID bodyID;
		bool activated = false;
	};

	struct PhysicsListenerContext
	{
		struct BodyBinding
		{
			entt::entity entity = entt::null;
			RigidbodyComponent* rigidbody = nullptr;
			JPH::uint64 userData = 0u;
		};

		void SetScene(Scene* scene);
		Scene* GetScene() const;

		void RegisterBody(JPH::BodyID bodyID, entt::entity entity, RigidbodyComponent* rigidbody, JPH::uint64 userData);
		void UnregisterBody(JPH::BodyID bodyID);
		BodyBinding LookupBinding(const JPH::BodyID& bodyID) const;
		void Clear();

	private:
		Scene* m_Scene = nullptr;
		mutable std::shared_mutex m_Mutex;
		std::unordered_map<JPH::BodyID, BodyBinding> m_BodyBindings;
	};

	using ContactValidationCallback = std::function<JPH::ValidateResult(const PhysicsContactData&)>;
	using ContactCallback = std::function<void(const PhysicsContactData&)>;
	using ActivationCallback = std::function<void(const PhysicsActivationData&)>;

	class JoltContactListener : public JPH::ContactListener
	{
	public:
		explicit JoltContactListener(Ref<PhysicsListenerContext> context);

		void SetContext(const Ref<PhysicsListenerContext>& context);

		JPH::ValidateResult OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult) override;
		void OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;
		void OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings) override;
		void OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair) override;

	private:
		using BodyBinding = PhysicsListenerContext::BodyBinding;

		void DispatchContact(PhysicsContactPhase phase, const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold* manifold, JPH::ContactSettings* settings, const JPH::SubShapeIDPair* subShapePair);
		PhysicsContactData MakeContactData(Scene* scene, PhysicsContactPhase phase, const BodyBinding& selfBinding, const BodyBinding& otherBinding, const JPH::Body* selfBody, const JPH::Body* otherBody, const JPH::ContactManifold* manifold, JPH::ContactSettings* settings, const JPH::CollideShapeResult* collisionResult, const JPH::SubShapeIDPair* subShapePair) const;
		static JPH::ValidateResult CombineValidateResult(JPH::ValidateResult lhs, JPH::ValidateResult rhs);
		static ContactCallback* SelectContactCallback(RigidbodyComponent& rb, PhysicsContactPhase phase);

		Ref<PhysicsListenerContext> m_Context;
	};

	class JoltBodyActivationListener : public JPH::BodyActivationListener
	{
	public:
		explicit JoltBodyActivationListener(Ref<PhysicsListenerContext> context);

		void SetContext(const Ref<PhysicsListenerContext>& context);

		void OnBodyActivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override;
		void OnBodyDeactivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData) override;

	private:
		void DispatchActivation(bool activated, const JPH::BodyID& bodyID, JPH::uint64 userData);

		Ref<PhysicsListenerContext> m_Context;
	};
}

#endif