// Copyright (c) 2025 Evangelion Manuhutu

#include "UUID.h"

#include <random>

namespace flex
{
	UUID::UUID()
		: m_UUID(GenerateUUID())
	{
	}

	UUID::UUID(const uint64_t uuid)
		: m_UUID(uuid)
	{
	}

	uint64_t UUID::GenerateUUID()
	{
		static std::random_device rd;
		return (static_cast<uint64_t>(rd()) << 32) | rd();
	}
}