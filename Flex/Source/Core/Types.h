// Copyright (c) 20245, Evangelion Manuhutu

#ifndef TYPES_H
#define TYPES_H

#include <memory>

namespace flex
{

	template<typename T>
	using Ref = std::shared_ptr<T>;

	template<typename T, typename... Args>
	Ref<T> CreateRef(Args &&... args)
	{
		return std::make_shared<T>(std::forward<Args>(args)...);
	}
}

#endif