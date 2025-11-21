// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef GLM_ENABLE_EXPERIMENTAL
	#define GLM_ENABLE_EXPERIMENTAL
#endif
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>

#include "JoltPhysics.h"
#include "JoltListeners.h"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Body/MotionProperties.h>

#include <algorithm>
#include <iostream>
#include <thread>
#include <cstdarg>

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
	
	using namespace JPH::literals;

	static JoltPhysics* s_JoltInstance = nullptr;

	void JoltPhysics::Init()
	{
		s_JoltInstance = new JoltPhysics();

		JPH::RegisterDefaultAllocator();
		
		JPH::Trace = TraceImpl;
		
		JPH::Factory::sInstance = new JPH::Factory();
		
		JPH::RegisterTypes();
		
		// Initialize temp allocator and job system (fallback ensures large scenes don't exhaust temporary memory)
		s_JoltInstance->tempAllocator = CreateScope<JPH::TempAllocatorImplWithMallocFallback>(10 * 1024 * 1024);
		const unsigned int hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
		const unsigned int workerThreads = hardwareThreads > 1 ? hardwareThreads - 1 : 1;
		s_JoltInstance->jobSystem = CreateScope<JPH::JobSystemThreadPool>(cMaxPhysicsJobs, cMaxPhysicsJobs, static_cast<int>(workerThreads));

		s_JoltInstance->listenerContext = CreateRef<PhysicsListenerContext>();
		s_JoltInstance->contactListener = CreateScope<JoltContactListener>(s_JoltInstance->listenerContext);
		s_JoltInstance->bodyActivationListener = CreateScope<JoltBodyActivationListener>(s_JoltInstance->listenerContext);
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
}