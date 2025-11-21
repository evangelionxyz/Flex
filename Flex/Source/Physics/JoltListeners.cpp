// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include <Jolt/Jolt.h>

#include "JoltListeners.h"

#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <mutex>
#include <shared_mutex>
#include <utility>

namespace flex
{
    void PhysicsListenerContext::SetScene(Scene* scene)
    {
        std::unique_lock<std::shared_mutex> lock(m_Mutex);
        m_Scene = scene;
    }

    Scene* PhysicsListenerContext::GetScene() const
    {
        std::shared_lock<std::shared_mutex> lock(m_Mutex);
        return m_Scene;
    }

    void PhysicsListenerContext::RegisterBody(JPH::BodyID bodyID, entt::entity entity, RigidbodyComponent* rigidbody, JPH::uint64 userData)
    {
        if (bodyID.IsInvalid())
        {
            return;
        }

        BodyBinding binding;
        binding.entity = entity;
        binding.rigidbody = rigidbody;
        binding.userData = userData;

        std::unique_lock<std::shared_mutex> lock(m_Mutex);
        m_BodyBindings[bodyID] = binding;
    }

    void PhysicsListenerContext::UnregisterBody(JPH::BodyID bodyID)
    {
        if (bodyID.IsInvalid())
        {
            return;
        }

        std::unique_lock<std::shared_mutex> lock(m_Mutex);
        m_BodyBindings.erase(bodyID);
    }

    PhysicsListenerContext::BodyBinding PhysicsListenerContext::LookupBinding(const JPH::BodyID& bodyID) const
    {
        if (bodyID.IsInvalid())
        {
            return {};
        }

        std::shared_lock<std::shared_mutex> lock(m_Mutex);
        auto it = m_BodyBindings.find(bodyID);
        if (it == m_BodyBindings.end())
        {
            return {};
        }

        return it->second;
    }

    void PhysicsListenerContext::Clear()
    {
        std::unique_lock<std::shared_mutex> lock(m_Mutex);
        m_BodyBindings.clear();
    }

    JoltContactListener::JoltContactListener(Ref<PhysicsListenerContext> context)
        : m_Context(std::move(context))
    {
    }

    void JoltContactListener::SetContext(const Ref<PhysicsListenerContext>& context)
    {
        m_Context = context;
    }

    JPH::ValidateResult JoltContactListener::OnContactValidate(const JPH::Body& inBody1, const JPH::Body& inBody2, JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult)
    {
        if (!m_Context)
        {
            return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        }

        auto binding1 = m_Context->LookupBinding(inBody1.GetID());
        auto binding2 = m_Context->LookupBinding(inBody2.GetID());
        Scene* scene = m_Context->GetScene();
        JPH::ValidateResult result = JPH::ValidateResult::AcceptAllContactsForThisBodyPair;

        if (binding1.rigidbody && binding1.rigidbody->onContactValidate)
        {
            PhysicsContactData data = MakeContactData(scene, PhysicsContactPhase::Validate, binding1, binding2, &inBody1, &inBody2, nullptr, nullptr, &inCollisionResult, nullptr);
            data.baseOffset = inBaseOffset;
            result = CombineValidateResult(result, binding1.rigidbody->onContactValidate(data));
        }

        if (binding2.rigidbody && binding2.rigidbody->onContactValidate)
        {
            PhysicsContactData data = MakeContactData(scene, PhysicsContactPhase::Validate, binding2, binding1, &inBody2, &inBody1, nullptr, nullptr, &inCollisionResult, nullptr);
            data.baseOffset = inBaseOffset;
            result = CombineValidateResult(result, binding2.rigidbody->onContactValidate(data));
        }

        return result;
    }

    void JoltContactListener::OnContactAdded(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
    {
        DispatchContact(PhysicsContactPhase::Enter, inBody1, inBody2, &inManifold, &ioSettings, nullptr);
    }

    void JoltContactListener::OnContactPersisted(const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
    {
        DispatchContact(PhysicsContactPhase::Persist, inBody1, inBody2, &inManifold, &ioSettings, nullptr);
    }

    void JoltContactListener::OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair)
    {
        if (!m_Context)
        {
            return;
        }

        Scene* scene = m_Context->GetScene();
        auto binding1 = m_Context->LookupBinding(inSubShapePair.GetBody1ID());
        auto binding2 = m_Context->LookupBinding(inSubShapePair.GetBody2ID());

        if (binding1.rigidbody)
        {
            if (auto* callback = SelectContactCallback(*binding1.rigidbody, PhysicsContactPhase::Exit); *callback)
            {
                PhysicsContactData data = MakeContactData(scene, PhysicsContactPhase::Exit, binding1, binding2, nullptr, nullptr, nullptr, nullptr, nullptr, &inSubShapePair);
                (*callback)(data);
            }
        }

        if (binding2.rigidbody)
        {
            if (auto* callback = SelectContactCallback(*binding2.rigidbody, PhysicsContactPhase::Exit); *callback)
            {
                PhysicsContactData data = MakeContactData(scene, PhysicsContactPhase::Exit, binding2, binding1, nullptr, nullptr, nullptr, nullptr, nullptr, &inSubShapePair);
                (*callback)(data);
            }
        }
    }

    void JoltContactListener::DispatchContact(PhysicsContactPhase phase, const JPH::Body& body1, const JPH::Body& body2, const JPH::ContactManifold* manifold, JPH::ContactSettings* settings, const JPH::SubShapeIDPair* subShapePair)
    {
        if (!m_Context)
        {
            return;
        }

        Scene* scene = m_Context->GetScene();
        auto binding1 = m_Context->LookupBinding(body1.GetID());
        auto binding2 = m_Context->LookupBinding(body2.GetID());

        if (binding1.rigidbody)
        {
            if (auto* callback = SelectContactCallback(*binding1.rigidbody, phase); *callback)
            {
                PhysicsContactData data = MakeContactData(scene, phase, binding1, binding2, &body1, &body2, manifold, settings, nullptr, subShapePair);
                (*callback)(data);
            }
        }

        if (binding2.rigidbody)
        {
            if (auto* callback = SelectContactCallback(*binding2.rigidbody, phase); *callback)
            {
                const JPH::ContactManifold* manifoldForBody2 = manifold;
                JPH::ContactManifold swappedManifold;
                if (manifold)
                {
                    swappedManifold = manifold->SwapShapes();
                    manifoldForBody2 = &swappedManifold;
                }

                PhysicsContactData data = MakeContactData(scene, phase, binding2, binding1, &body2, &body1, manifoldForBody2, settings, nullptr, subShapePair);
                (*callback)(data);
            }
        }
    }

    PhysicsContactData JoltContactListener::MakeContactData(Scene* scene, PhysicsContactPhase phase, const BodyBinding& selfBinding, const BodyBinding& otherBinding, const JPH::Body* selfBody, const JPH::Body* otherBody, const JPH::ContactManifold* manifold, JPH::ContactSettings* settings, const JPH::CollideShapeResult* collisionResult, const JPH::SubShapeIDPair* subShapePair) const
    {
        PhysicsContactData data;
        data.scene = scene;
        data.phase = phase;
        data.selfEntity = selfBinding.entity;
        data.otherEntity = otherBinding.entity;
        data.selfRigidbody = selfBinding.rigidbody;
        data.otherRigidbody = otherBinding.rigidbody;
        data.selfBody = selfBody;
        data.otherBody = otherBody;
        data.manifold = manifold;
        data.settings = settings;
        data.collisionResult = collisionResult;
        data.subShapePair = subShapePair;
        return data;
    }

    JPH::ValidateResult JoltContactListener::CombineValidateResult(JPH::ValidateResult lhs, JPH::ValidateResult rhs)
    {
        return static_cast<int>(rhs) > static_cast<int>(lhs) ? rhs : lhs;
    }

    ContactCallback* JoltContactListener::SelectContactCallback(RigidbodyComponent& rb, PhysicsContactPhase phase)
    {
        switch (phase)
        {
        case PhysicsContactPhase::Enter: return &rb.onContactEnter;
        case PhysicsContactPhase::Persist: return &rb.onContactPersist;
        case PhysicsContactPhase::Exit: return &rb.onContactExit;
        default: break;
        }

        return nullptr;
    }

    JoltBodyActivationListener::JoltBodyActivationListener(Ref<PhysicsListenerContext> context)
        : m_Context(std::move(context))
    {
    }

    void JoltBodyActivationListener::SetContext(const Ref<PhysicsListenerContext>& context)
    {
        m_Context = context;
    }

    void JoltBodyActivationListener::OnBodyActivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData)
    {
        DispatchActivation(true, inBodyID, inBodyUserData);
    }

    void JoltBodyActivationListener::OnBodyDeactivated(const JPH::BodyID& inBodyID, JPH::uint64 inBodyUserData)
    {
        DispatchActivation(false, inBodyID, inBodyUserData);
    }

    void JoltBodyActivationListener::DispatchActivation(bool activated, const JPH::BodyID& bodyID, JPH::uint64 userData)
    {
        if (!m_Context)
        {
            return;
        }

        Scene* scene = m_Context->GetScene();
        auto binding = m_Context->LookupBinding(bodyID);
        if (!binding.rigidbody)
        {
            return;
        }

        PhysicsActivationData data;
        data.scene = scene;
        data.entity = binding.entity;
        data.rigidbody = binding.rigidbody;
        data.bodyID = bodyID;
        data.activated = activated;

        ActivationCallback* callback = activated ? &binding.rigidbody->onBodyActivated : &binding.rigidbody->onBodyDeactivated;
        if (callback && *callback)
        {
            (*callback)(data);
        }
    }
}