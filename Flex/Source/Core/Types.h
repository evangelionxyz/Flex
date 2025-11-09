// Copyright (c) 20245, Evangelion Manuhutu

#ifndef TYPES_H
#define TYPES_H

#include <memory>

namespace flex
{
	// Shared Pointer
	template<typename T>
	using Ref = std::shared_ptr<T>;

	template<typename T, typename... Args>
	Ref<T> CreateRef(Args &&... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}

	// Unique Pointer
	template<typename T>
	using Scope = std::unique_ptr<T>;

	template<typename T, typename... Args>
	Scope<T> CreateScope(Args &&... args)
	{
		return std::make_unique<T>(std::forward<Args>(args)...);
	}
}

#endif