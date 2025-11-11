// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "JoltPhysicsScene.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"

#include <SDL3/SDL_log.h>

namespace flex
{
    JoltPhysicsScene::JoltPhysicsScene(Scene* scene)
		: m_Scene(scene)
	{
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
        auto physics = JoltPhysics::Get();
		if (!physics)
		{
			return;
		}

		if (physics->listenerContext)
		{
			physics->listenerContext->Clear();
			physics->listenerContext->SetScene(m_Scene);
			if (physics->contactListener)
			{
				auto* listener = dynamic_cast<JoltContactListener*>(physics->contactListener.get());
				if (listener)
				{
					listener->SetContext(physics->listenerContext);
				}
			}
			if (physics->bodyActivationListener)
			{
				auto* listener = dynamic_cast<JoltBodyActivationListener*>(physics->bodyActivationListener.get());
				if (listener)
				{
					listener->SetContext(physics->listenerContext);
				}
			}
		}

		m_PhysicsSystem.Init(cNumBodies, cNumBodyMutexes, cMaxBodyPairs,
			cMaxContactConstraints, physics->broadPhaseLayer,
			physics->objectVsBroadPhaseLayerFilter,
			physics->objectLayerPairFilter);

		m_PhysicsSystem.SetBodyActivationListener(physics->bodyActivationListener.get());
		m_PhysicsSystem.SetContactListener(physics->contactListener.get());
		m_PhysicsSystem.OptimizeBroadPhase();
		m_PhysicsSystem.SetGravity(GlmToJoltVec3(m_Scene->sceneGravity));

		m_BodyInterface = &m_PhysicsSystem.GetBodyInterface();

		auto view = m_Scene->registry->view<TransformComponent, RigidbodyComponent>();
		view.each([this](entt::entity entity, TransformComponent&, RigidbodyComponent& rb)
		{
			if (!rb.bodyID.IsInvalid())
			{
				DestroyEntity(entity);
			}
			InstantiateEntity(entity);
		});
	}

	void JoltPhysicsScene::SimulationStop()
	{
		auto view = m_Scene->registry->view<TransformComponent, RigidbodyComponent>();
		view.each([this](entt::entity entity, TransformComponent&, RigidbodyComponent&)
		{
			DestroyEntity(entity);
		});

		auto physics = JoltPhysics::Get();
		if (physics && physics->listenerContext)
		{
			physics->listenerContext->Clear();
			physics->listenerContext->SetScene(nullptr);
		}
	}

	void JoltPhysicsScene::Simulate(float deltaTime)
	{
        auto physics = JoltPhysics::Get();
		if (!physics || deltaTime <= 0.0f)
		{
			return;
		}

		JPH::TempAllocator* tempAllocator = physics->tempAllocator.get();
		JPH::JobSystem* jobSystem = physics->jobSystem.get();
		if (!tempAllocator || !jobSystem)
		{
			return;
		}

		const int collisionSteps = 1;
		m_PhysicsSystem.Update(deltaTime, collisionSteps, tempAllocator, jobSystem);

		auto view = m_Scene->registry->view<TransformComponent, RigidbodyComponent>();
		view.each([this](TransformComponent& transform, RigidbodyComponent& rb)
		{
			if (rb.isStatic || rb.bodyID.IsInvalid())
			{
				return;
			}

			transform.position = GetPosition(rb.bodyID);
			transform.rotation = GetEulerAngles(rb.bodyID);
		});
	}

	JPH::BodyCreationSettings JoltPhysicsScene::CreateBody(JPH::ShapeRefC shape, RigidbodyComponent& rb, const glm::vec3& position, const glm::quat& rotation)
	{
		JPH::EMotionType motionType = rb.isStatic ? JPH::EMotionType::Static : JPH::EMotionType::Dynamic;
		JPH::ObjectLayer objectLayer = rb.isStatic ? PhysicsLayers::NON_MOVING : PhysicsLayers::MOVING;

		JPH::BodyCreationSettings settings(
			shape,
			GlmToJoltVec3(position),
			GlmToJoltQuat(rotation),
			motionType,
			objectLayer
		);

		settings.mAllowSleeping = rb.allowSleeping;
		settings.mGravityFactor = rb.useGravity ? rb.gravityFactor : 0.0f;

		JPH::EAllowedDOFs allowed = JPH::EAllowedDOFs::None;
		if (rb.moveX) allowed |= JPH::EAllowedDOFs::TranslationX;
		if (rb.moveY) allowed |= JPH::EAllowedDOFs::TranslationY;
		if (rb.moveZ) allowed |= JPH::EAllowedDOFs::TranslationZ;
		if (rb.rotateX) allowed |= JPH::EAllowedDOFs::RotationX;
		if (rb.rotateY) allowed |= JPH::EAllowedDOFs::RotationY;
		if (rb.rotateZ) allowed |= JPH::EAllowedDOFs::RotationZ;
		settings.mAllowedDOFs = allowed == JPH::EAllowedDOFs::None ? JPH::EAllowedDOFs::All : allowed;

		if (!rb.isStatic)
		{
			settings.mMotionQuality = rb.MotionQuality == RigidbodyComponent::EMotionQuality::LinearCast
				? JPH::EMotionQuality::LinearCast
				: JPH::EMotionQuality::Discrete;
			settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
			settings.mMassPropertiesOverride.mMass = std::max(rb.mass, 0.0001f);
			//settings.mMassPropertiesOverride.mCenterOfMass = GlmToJoltVec3(rb.centerOfMass);
		}
		else
		{
			settings.mMotionQuality = JPH::EMotionQuality::Discrete;
		}

		return settings;
	}

	void JoltPhysicsScene::InstantiateEntity(entt::entity entity)
	{
		if (!m_Scene->HasComponent<TransformComponent>(entity) ||
			!m_Scene->HasComponent<RigidbodyComponent>(entity) ||
			!m_Scene->HasComponent<BoxColliderComponent>(entity))
		{
			return;
		}

		auto& transform = m_Scene->GetComponent<TransformComponent>(entity);
		auto& rb = m_Scene->GetComponent<RigidbodyComponent>(entity);
		auto& boxCollider = m_Scene->GetComponent<BoxColliderComponent>(entity);

		if (!rb.bodyID.IsInvalid())
		{
			DestroyEntity(entity);
		}

		const glm::vec3 scaledSize = glm::abs(transform.scale) * boxCollider.scale;
		const glm::vec3 halfExtents = scaledSize;
		if (halfExtents.x <= 0.0f || halfExtents.y <= 0.0f || halfExtents.z <= 0.0f)
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Box collider has non-positive extents, skipping body creation");
			return;
		}
        
		JPH::BoxShapeSettings shapeSettings(GlmToJoltVec3(halfExtents));
		shapeSettings.mDensity = std::max(boxCollider.density, 0.0001f);
		JPH::ShapeSettings::ShapeResult shapeResult = shapeSettings.Create();
		if (shapeResult.HasError())
		{
            const char *errorMessage = shapeResult.GetError().c_str();
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create shape %s\n", errorMessage);
			return;
		}
        
		JPH::ShapeRefC shape = shapeResult.Get();
		if (!shape)
		{
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed Failed to create box shape instance\n");
			return;
		}

		boxCollider.shape = (void*)shape.GetPtr();

		const glm::quat rotation = glm::quat(glm::radians(transform.rotation));
		const glm::vec3 offset = rotation * (boxCollider.offset * transform.scale);
		const glm::vec3 bodyPosition = transform.position + offset;

		JPH::BodyCreationSettings bodySettings = CreateBody(shape, rb, bodyPosition, rotation);
		bodySettings.mUserData = static_cast<JPH::uint64>(entt::to_integral(entity));
		bodySettings.mFriction = boxCollider.friction;
		bodySettings.mRestitution = boxCollider.restitution;
        
		JPH::BodyID bodyID = m_BodyInterface->CreateAndAddBody(bodySettings, rb.isStatic ? JPH::EActivation::DontActivate : JPH::EActivation::Activate);
		if (bodyID.IsInvalid())
		{
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create physics body for entity\n");
			return;
		}

		rb.bodyID = bodyID;

		auto physics = JoltPhysics::Get();
		if (physics && physics->listenerContext)
		{
			physics->listenerContext->RegisterBody(bodyID, entity, &rb, bodySettings.mUserData);
		}
	}

	void JoltPhysicsScene::DestroyEntity(entt::entity entity)
	{
		if (!m_Scene->HasComponent<RigidbodyComponent>(entity))
		{
			return;
		}

		auto& rb = m_Scene->GetComponent<RigidbodyComponent>(entity);
		if (rb.bodyID.IsInvalid())
		{
			return;
		}

		auto physics = JoltPhysics::Get();
		if (physics && physics->listenerContext)
		{
			physics->listenerContext->UnregisterBody(rb.bodyID);
		}

		m_BodyInterface->RemoveBody(rb.bodyID);
		m_BodyInterface->DestroyBody(rb.bodyID);
		rb.bodyID = JPH::BodyID();

		if (m_Scene->HasComponent<BoxColliderComponent>(entity))
		{
			m_Scene->GetComponent<BoxColliderComponent>(entity).shape = nullptr;
		}
	}

	void JoltPhysicsScene::CreateBoxCollider(entt::entity entity)
	{
		if (!m_Scene->HasComponent<RigidbodyComponent>(entity) || !m_Scene->HasComponent<BoxColliderComponent>(entity))
		{
			return;
		}

		DestroyEntity(entity);
		InstantiateEntity(entity);
	}

	void JoltPhysicsScene::CreateSphereCollider(entt::entity entity)
	{
		// TODO: Implement sphere collider support
	}

	void JoltPhysicsScene::AddForce(JPH::BodyID bodyID, const glm::vec3& force)
	{
		m_BodyInterface->AddForce(bodyID, GlmToJoltVec3(force));
	}

	void JoltPhysicsScene::AddTorque(JPH::BodyID bodyID, const glm::vec3& torque)
	{
		m_BodyInterface->AddTorque(bodyID, GlmToJoltVec3(torque));
	}

	void JoltPhysicsScene::AddForceAndTorque(JPH::BodyID bodyID, const glm::vec3& force, const glm::vec3& torque)
	{
		m_BodyInterface->AddForce(bodyID, GlmToJoltVec3(force));
		m_BodyInterface->AddTorque(bodyID, GlmToJoltVec3(torque));
	}

	void JoltPhysicsScene::AddAngularImpulse(JPH::BodyID bodyID, const glm::vec3& impulse)
	{
		m_BodyInterface->AddAngularImpulse(bodyID, GlmToJoltVec3(impulse));
	}

	void JoltPhysicsScene::ActivateBody(JPH::BodyID bodyID)
	{
		m_BodyInterface->ActivateBody(bodyID);
	}

	void JoltPhysicsScene::DeactivateBody(JPH::BodyID bodyID)
	{
		m_BodyInterface->DeactivateBody(bodyID);
	}

	void JoltPhysicsScene::DestroyBody(JPH::BodyID bodyID)
	{
		auto physics = JoltPhysics::Get();
		if (physics && physics->listenerContext)
		{
			physics->listenerContext->UnregisterBody(bodyID);
		}

		m_BodyInterface->RemoveBody(bodyID);
		m_BodyInterface->DestroyBody(bodyID);
	}

	bool JoltPhysicsScene::IsActive(JPH::BodyID bodyID)
	{
		return !bodyID.IsInvalid() && m_BodyInterface->IsActive(bodyID);
	}

	void JoltPhysicsScene::MoveKinematic(JPH::BodyID bodyID, const glm::vec3& targetPosition, const glm::vec3& targetRotation, float deltaTime)
	{
		glm::quat rotation = glm::quat(glm::radians(targetRotation));
		m_BodyInterface->MoveKinematic(bodyID, GlmToJoltVec3(targetPosition), GlmToJoltQuat(rotation), deltaTime);
	}

	void JoltPhysicsScene::AddImpulse(JPH::BodyID bodyID, const glm::vec3& impulse)
	{
		m_BodyInterface->AddImpulse(bodyID, GlmToJoltVec3(impulse));
	}

	void JoltPhysicsScene::AddLinearVelocity(JPH::BodyID bodyID, const glm::vec3& velocity)
	{
		m_BodyInterface->AddLinearVelocity(bodyID, GlmToJoltVec3(velocity));
	}

	void JoltPhysicsScene::SetPosition(JPH::BodyID bodyID, const glm::vec3& position, bool activate)
	{
		m_BodyInterface->SetPosition(bodyID, GlmToJoltVec3(position), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
	}

	void JoltPhysicsScene::SetEulerAngleRotation(JPH::BodyID bodyID, const glm::vec3& rotation, bool activate)
	{
		glm::quat quat = glm::quat(glm::radians(rotation));
		m_BodyInterface->SetRotation(bodyID, GlmToJoltQuat(quat), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
	}

	void JoltPhysicsScene::SetRotation(JPH::BodyID bodyID, const glm::quat& rotation, bool activate)
	{
		m_BodyInterface->SetRotation(bodyID, GlmToJoltQuat(rotation), activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
	}

	void JoltPhysicsScene::SetLinearVelocity(JPH::BodyID bodyID, const glm::vec3& vel)
	{
		m_BodyInterface->SetLinearVelocity(bodyID, GlmToJoltVec3(vel));
	}

	void JoltPhysicsScene::SetFriction(JPH::BodyID bodyID, float value)
	{
		m_BodyInterface->SetFriction(bodyID, value);
	}

	void JoltPhysicsScene::SetRestitution(JPH::BodyID bodyID, float value)
	{
		m_BodyInterface->SetRestitution(bodyID, value);
	}

	void JoltPhysicsScene::SetGravityFactor(JPH::BodyID bodyID, float value)
	{
		m_BodyInterface->SetGravityFactor(bodyID, value);
	}

	float JoltPhysicsScene::GetRestitution(JPH::BodyID bodyID)
	{
		return m_BodyInterface->GetRestitution(bodyID);
	}

	float JoltPhysicsScene::GetFriction(JPH::BodyID bodyID)
	{
		return m_BodyInterface->GetFriction(bodyID);
	}

	float JoltPhysicsScene::GetGravityFactor(JPH::BodyID bodyID)
	{
		JPH::BodyLockRead lock(m_PhysicsSystem.GetBodyLockInterface(), bodyID);
		if (!lock.Succeeded())
		{
			return 1.0f;
		}

		const JPH::Body& body = lock.GetBody();
		const JPH::MotionProperties* motion = body.GetMotionProperties();
		return motion ? motion->GetGravityFactor() : 1.0f;
	}

	glm::vec3 JoltPhysicsScene::GetPosition(JPH::BodyID bodyID)
	{
		return JoltToGlmVec3(m_BodyInterface->GetPosition(bodyID));
	}

	glm::vec3 JoltPhysicsScene::GetEulerAngles(JPH::BodyID bodyID)
	{
		glm::quat rotation = GetRotation(bodyID);
		return glm::degrees(glm::eulerAngles(rotation));
	}

	glm::quat JoltPhysicsScene::GetRotation(JPH::BodyID bodyID)
	{
		return JoltToGlmQuat(m_BodyInterface->GetRotation(bodyID));
	}

	glm::vec3 JoltPhysicsScene::GetCenterOfMassPosition(JPH::BodyID bodyID)
	{
		return JoltToGlmVec3(m_BodyInterface->GetCenterOfMassPosition(bodyID));
	}

	glm::vec3 JoltPhysicsScene::GetLinearVelocity(JPH::BodyID bodyID)
	{
		return JoltToGlmVec3(m_BodyInterface->GetLinearVelocity(bodyID));
	}

	void JoltPhysicsScene::SetMaxLinearVelocity(JPH::BodyID bodyID, float max)
	{
		JPH::BodyLockWrite lock(m_PhysicsSystem.GetBodyLockInterface(), bodyID);
		if (!lock.Succeeded())
		{
			return;
		}

		JPH::MotionProperties* motion = lock.GetBody().GetMotionProperties();
		if (motion)
		{
			motion->SetMaxLinearVelocity(max);
		}
	}

	void JoltPhysicsScene::SetMaxAngularVelocity(JPH::BodyID bodyID, float max)
	{
		JPH::BodyLockWrite lock(m_PhysicsSystem.GetBodyLockInterface(), bodyID);
		if (!lock.Succeeded())
		{
			return;
		}

		JPH::MotionProperties* motion = lock.GetBody().GetMotionProperties();
		if (motion)
		{
			motion->SetMaxAngularVelocity(max);
		}
	}
}