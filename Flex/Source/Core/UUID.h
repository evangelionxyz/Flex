// Copyright (c) 2025 Evangelion Manuhutu

#ifndef UUID_H
#define UUID_H

#include <functional>
#include <cstdint>

namespace flex
{
	class UUID
	{
	public:
		UUID();
		explicit UUID(uint64_t uuid);
		UUID(const UUID& uuid) = default;

		operator uint64_t() const { return m_UUID; }
	private:
		uint64_t m_UUID;
	};
}

template<>
struct std::hash<flex::UUID>
{
	std::size_t operator() (const flex::UUID& uuid) const noexcept
	{
		return hash<uint64_t>()(uuid);
	}
};

#endif