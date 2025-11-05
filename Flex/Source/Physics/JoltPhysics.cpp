// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "JoltPhysics.h"

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
			s_JoltInstance->objectVsBroadPhaseLayerFilter, s_JoltInstance->objectLayerPairFilter);

		// m_PhysicsSystem.SetBodyActivationListener(s_JoltInstance->bodyActivationListener.get());
		// m_PhysicsSystem.SetContactListener(s_JoltInstance->contactListener.get());
		m_PhysicsSystem.SetBodyActivationListener(s_JoltInstance->bodyActivationListener.get());
		m_PhysicsSystem.SetContactListener(s_JoltInstance->contactListener.get());
		m_PhysicsSystem.SetGravity(GlmToJoltVec3(glm::vec3(0.0f, -9.8f, 0.0f)));
		m_PhysicsSystem.OptimizeBroadPhase();

		m_BodyInterface = &m_PhysicsSystem.GetBodyInterface();
	}

	JoltPhysicsScene::~JoltPhysicsScene()
	{
	}

	void JoltPhysicsScene::SimulationStart()
	{
	}

	void JoltPhysicsScene::SimulationStop()
	{

	}

	void JoltPhysicsScene::Simulate(float deltaTime)
	{
	}
}