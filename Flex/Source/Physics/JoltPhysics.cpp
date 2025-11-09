// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef GLM_ENABLE_EXPERIMENTAL
	#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>

#include "JoltPhysics.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"

namespace flex
{
	static void TraceImpl(const char* inFMT, ...)
	{
		// Format the message
		va_list list;
		va_start(list, inFMT);
		char buffer[1024];
		vsnprintf(buffer, sizeof(buffer), inFMT, list);
		va_end(list);

		// Print to the TTY
		std::cout << buffer << "\n";
	}

	static constexpr int cMaxPhysicsJobs = 2048;
	static constexpr unsigned int cNumBodies = 20480;
	static constexpr unsigned int cNumBodyMutexes = 0;
	static constexpr unsigned int cMaxBodyPairs = 64000;
	static constexpr unsigned int cMaxContactConstraints = 20480;

	using namespace JPH::literals;

	static JoltPhysics* s_JoltInstance = nullptr;

	void JoltPhysics::Init()
	{
		s_JoltInstance = new JoltPhysics();

		JPH::RegisterDefaultAllocator();
		
		JPH::Trace = TraceImpl;
		
		JPH::Factory::sInstance = new JPH::Factory();
		
		JPH::RegisterTypes();
		
		// Initialize temp allocator and job system
		s_JoltInstance->tempAllocator = CreateScope<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
		s_JoltInstance->jobSystem = CreateScope<JPH::JobSystemThreadPool>(cMaxPhysicsJobs, cMaxPhysicsJobs,
			std::thread::hardware_concurrency() - 1);

		s_JoltInstance->contactListener = CreateScope<JoltContactListener>();
		s_JoltInstance->bodyActivationListener = CreateScope<JoltBodyActivationListener>();
	}

	void JoltPhysics::Shutdown()
	{
		JPH::UnregisterTypes();

		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;

		delete s_JoltInstance;
		s_JoltInstance = nullptr;
	}

	JoltPhysics* JoltPhysics::Get()
	{
		return s_JoltInstance;
	}

	JoltPhysicsScene::JoltPhysicsScene(Scene* scene)
		: m_Scene(scene)
	{
		m_PhysicsSystem.Init(cNumBodies, cNumBodyMutexes, cMaxBodyPairs,
			cMaxContactConstraints, s_JoltInstance->broadPhaseLayer,
			s_JoltInstance->objectVsBroadPhaseLayerFilter,
			s_JoltInstance->objectLayerPairFilter);

		// m_PhysicsSystem.SetBodyActivationListener(s_JoltInstance->bodyActivationListener.get());
		// m_PhysicsSystem.SetContactListener(s_JoltInstance->contactListener.get());
		m_PhysicsSystem.SetBodyActivationListener(s_JoltInstance->bodyActivationListener.get());
		m_PhysicsSystem.SetContactListener(s_JoltInstance->contactListener.get());
		m_PhysicsSystem.OptimizeBroadPhase();

		m_BodyInterface = &m_PhysicsSystem.GetBodyInterface();
	}

	JoltPhysicsScene::~JoltPhysicsScene()
	{
	}

    Ref<JoltPhysicsScene> JoltPhysicsScene::Create(Scene *scene)
    {
        return CreateRef<JoltPhysicsScene>(scene);
    }

    void JoltPhysicsScene::SimulationStart()
	{
		m_PhysicsSystem.SetGravity(GlmToJoltVec3(m_Scene->sceneGravity));

		auto view = m_Scene->registry->view<TransformComponent, RigidbodyComponent>();
		view.each([this](auto entity, auto &transform, auto &rb)
		{
			InstantiateEntity(entity);
		});
	}

	void JoltPhysicsScene::SimulationStop()
	{
		auto view = m_Scene->registry->view<TransformComponent, RigidbodyComponent>();
		view.each([this](auto entity, auto &transform, auto &rb)
		{
			DestroyEntity(entity);
		});
	}

	void JoltPhysicsScene::Simulate(float deltaTime)
	{
		// Step the physics simulation
		const int collisionSteps = 1;
		m_PhysicsSystem.Update(deltaTime, collisionSteps, s_JoltInstance->tempAllocator.get(), s_JoltInstance->jobSystem.get());

		// Sync physics transforms back to components
		auto view = m_Scene->registry->view<TransformComponent, RigidbodyComponent>();
		view.each([this](auto entity, auto& transform, auto& rb) {
		if (rb.body && !rb.isStatic)
		{
			transform.position = GetPosition(*rb.body);
			transform.rotation = GetEulerAngles(*rb.body);
		}
		});
	}

	JPH::BodyCreationSettings JoltPhysicsScene::CreateBody(JPH::ShapeRefC shape, RigidbodyComponent &rb, const glm::vec3 &position, const glm::quat &rotation)
	{
		// Determine motion type based on rigidbody settings
		JPH::EMotionType motionType = rb.isStatic ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic;
		JPH::ObjectLayer objectLayer = rb.isStatic ? PhysicsLayers::NON_MOVING : PhysicsLayers::MOVING;

		// Create body settings
		JPH::BodyCreationSettings settings(
			shape,
			GlmToJoltVec3(position),
			GlmToJoltQuat(rotation),
			motionType,
			objectLayer
		);

		// Configure rigidbody properties
		if (!rb.isStatic)
		{
			settings.mAllowSleeping = rb.allowSleeping;
			settings.mGravityFactor = rb.gravityFactor;

			// Set motion quality
			if (rb.MotionQuality == RigidbodyComponent::EMotionQuality::LinearCast)
				settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
			else
				settings.mMotionQuality = JPH::EMotionQuality::Discrete;

			// Configure mass properties
			settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
			settings.mMassPropertiesOverride.mMass = rb.mass;
		}

		return settings;
	}

	void JoltPhysicsScene::InstantiateEntity(entt::entity entity)
	{
		if (!m_Scene->HasComponent<TransformComponent>(entity))
			return;

		auto& transform = m_Scene->GetComponent<TransformComponent>(entity);

		// Check if entity has rigidbody and box collider
		if (m_Scene->HasComponent<RigidbodyComponent>(entity) && m_Scene->HasComponent<BoxColliderComponent>(entity))
		{
			auto& rb = m_Scene->GetComponent<RigidbodyComponent>(entity);
			auto& boxCollider = m_Scene->GetComponent<BoxColliderComponent>(entity);

			// Create box shape
			glm::vec3 halfExtents = boxCollider.scale * 0.5f;
			JPH::BoxShapeSettings shapeSettings(GlmToJoltVec3(halfExtents));
			shapeSettings.mDensity = boxCollider.density;
			JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
			JPH::ShapeRefC shape = shapeResult.Get();

			if (!shape)
			{
				std::cerr << "Failed to create box shape for entity\n";
				return;
			}

			// Store shape pointer
			boxCollider.shape = (void*)shape.GetPtr();

			// Convert euler angles to quaternion
			glm::quat rotation = glm::quat(glm::radians(transform.rotation));

			// Create body settings
			JPH::BodyCreationSettings bodySettings = CreateBody(shape, rb, transform.position, rotation);

			// Apply friction and restitution from collider
			bodySettings.mFriction = boxCollider.friction;
			bodySettings.mRestitution = boxCollider.restitution;

			// Create and add body to physics system
			JPH::Body* body = m_BodyInterface->CreateBody(bodySettings);
			if (body)
			{
				rb.body = body;
				m_BodyInterface->AddBody(body->GetID(), JPH::EActivation::Activate);

				// Apply axis locks if needed
				if (!rb.isStatic)
				{
					// Lock translation axes
					if (!rb.moveX || !rb.moveY || !rb.moveZ)
					{
						JPH::Vec3 allowedDOFs(
							rb.moveX ? 1.0f : 0.0f,
							rb.moveY ? 1.0f : 0.0f,
							rb.moveZ ? 1.0f : 0.0f
						);
					}

					if (!rb.rotateX || !rb.rotateY || !rb.rotateZ)
					{
					}
				}
			}
		}
	}

	void JoltPhysicsScene::DestroyEntity(entt::entity entity)
	{
		if (!m_Scene->HasComponent<RigidbodyComponent>(entity))
			return;

		auto& rb = m_Scene->GetComponent<RigidbodyComponent>(entity);
		if (rb.body)
		{
			m_BodyInterface->RemoveBody(rb.body->GetID());
			m_BodyInterface->DestroyBody(rb.body->GetID());
			rb.body = nullptr;
		}
	}

	void JoltPhysicsScene::CreateBoxCollider(entt::entity entity)
	{
		// This is typically called when a box collider component is added
		// If the entity already has a body, we need to recreate it with the new shape
		if (m_Scene->HasComponent<RigidbodyComponent>(entity))
		{
			auto& rb = m_Scene->GetComponent<RigidbodyComponent>(entity);
			if (rb.body)
			{
				// Destroy existing body and recreate with box collider
				DestroyEntity(entity);
			}
			InstantiateEntity(entity);
		}
	}

	void JoltPhysicsScene::CreateSphereCollider(entt::entity entity)
	{
		// TODO: Implement sphere collider support
		// Similar to CreateBoxCollider but using SphereShape
	}

	void JoltPhysicsScene::AddForce(const JPH::Body& body, const glm::vec3& force)
	{
		m_BodyInterface->AddForce(body.GetID(), GlmToJoltVec3(force));
	}

	void JoltPhysicsScene::AddTorque(const JPH::Body& body, const glm::vec3& torque)
	{
		m_BodyInterface->AddTorque(body.GetID(), GlmToJoltVec3(torque));
	}

	void JoltPhysicsScene::AddForceAndTorque(const JPH::Body& body, const glm::vec3& force, const glm::vec3& torque)
	{
		m_BodyInterface->AddForce(body.GetID(), GlmToJoltVec3(force));
		m_BodyInterface->AddTorque(body.GetID(), GlmToJoltVec3(torque));
	}

	void JoltPhysicsScene::AddAngularImpulse(const JPH::Body& body, const glm::vec3& impulse)
	{
		m_BodyInterface->AddAngularImpulse(body.GetID(), GlmToJoltVec3(impulse));
	}

	void JoltPhysicsScene::ActivateBody(const JPH::Body& body)
	{
		m_BodyInterface->ActivateBody(body.GetID());
	}

	void JoltPhysicsScene::DeactivateBody(const JPH::Body& body)
	{
		m_BodyInterface->DeactivateBody(body.GetID());
	}

	void JoltPhysicsScene::DestroyBody(const JPH::Body& body)
	{
		m_BodyInterface->RemoveBody(body.GetID());
		m_BodyInterface->DestroyBody(body.GetID());
	}

	bool JoltPhysicsScene::IsActive(const JPH::Body& body)
	{
		return m_BodyInterface->IsActive(body.GetID());
	}

	void JoltPhysicsScene::MoveKinematic(const JPH::Body& body, const glm::vec3& targetPosition, const glm::vec3& targetRotation, float deltaTime)
	{
		glm::quat rotation = glm::quat(glm::radians(targetRotation));
		m_BodyInterface->MoveKinematic(body.GetID(), GlmToJoltVec3(targetPosition), GlmToJoltQuat(rotation), deltaTime);
	}

	void JoltPhysicsScene::AddImpulse(const JPH::Body& body, const glm::vec3& impulse)
	{
		m_BodyInterface->AddImpulse(body.GetID(), GlmToJoltVec3(impulse));
	}

	void JoltPhysicsScene::AddLinearVelocity(const JPH::Body& body, const glm::vec3& velocity)
	{
		m_BodyInterface->AddLinearVelocity(body.GetID(), GlmToJoltVec3(velocity));
	}

	void JoltPhysicsScene::SetPosition(const JPH::Body& body, const glm::vec3& position, bool activate)
	{
		m_BodyInterface->SetPosition(body.GetID(), GlmToJoltVec3(position), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
	}

	void JoltPhysicsScene::SetEulerAngleRotation(const JPH::Body& body, const glm::vec3& rotation, bool activate)
	{
		glm::quat quat = glm::quat(glm::radians(rotation));
		m_BodyInterface->SetRotation(body.GetID(), GlmToJoltQuat(quat), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
	}

	void JoltPhysicsScene::SetRotation(const JPH::Body& body, const glm::quat& rotation, bool activate)
	{
		m_BodyInterface->SetRotation(body.GetID(), GlmToJoltQuat(rotation), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
	}

	void JoltPhysicsScene::SetLinearVelocity(const JPH::Body& body, const glm::vec3& vel)
	{
		m_BodyInterface->SetLinearVelocity(body.GetID(), GlmToJoltVec3(vel));
	}

	void JoltPhysicsScene::SetFriction(const JPH::Body& body, float value)
	{
		m_BodyInterface->SetFriction(body.GetID(), value);
	}

	void JoltPhysicsScene::SetRestitution(const JPH::Body& body, float value)
	{
		m_BodyInterface->SetRestitution(body.GetID(), value);
	}

	void JoltPhysicsScene::SetGravityFactor(const JPH::Body& body, float value)
	{
		m_BodyInterface->SetGravityFactor(body.GetID(), value);
	}

	float JoltPhysicsScene::GetRestitution(const JPH::Body& body)
	{
		return m_BodyInterface->GetRestitution(body.GetID());
	}

	float JoltPhysicsScene::GetFriction(const JPH::Body& body)
	{
		return m_BodyInterface->GetFriction(body.GetID());
	}

	float JoltPhysicsScene::GetGravityFactor(const JPH::Body& body)
	{
		if (body.GetMotionProperties() != nullptr)
			return body.GetMotionProperties()->GetGravityFactor();
		return 1.0f;
	}

	glm::vec3 JoltPhysicsScene::GetPosition(const JPH::Body& body)
	{
		return JoltToGlmVec3(m_BodyInterface->GetPosition(body.GetID()));
	}

	glm::vec3 JoltPhysicsScene::GetEulerAngles(const JPH::Body& body)
	{
		JPH::Quat joltQuat = m_BodyInterface->GetRotation(body.GetID());
		glm::quat quat = JoltToGlmQuat(joltQuat);
		return glm::degrees(glm::eulerAngles(quat));
	}

	glm::quat JoltPhysicsScene::GetRotation(const JPH::Body& body)
	{
		return JoltToGlmQuat(m_BodyInterface->GetRotation(body.GetID()));
	}

	glm::vec3 JoltPhysicsScene::GetCenterOfMassPosition(const JPH::Body& body)
	{
		return JoltToGlmVec3(m_BodyInterface->GetCenterOfMassPosition(body.GetID()));
	}

	glm::vec3 JoltPhysicsScene::GetLinearVelocity(const JPH::Body& body)
	{
		return JoltToGlmVec3(m_BodyInterface->GetLinearVelocity(body.GetID()));
	}

	void JoltPhysicsScene::SetMaxLinearVelocity(JPH::Body& body, float max)
	{
		body.GetMotionProperties()->SetMaxLinearVelocity(max);
	}

	void JoltPhysicsScene::SetMaxAngularVelocity(JPH::Body& body, float max)
	{
		body.GetMotionProperties()->SetMaxAngularVelocity(max);
	}
}