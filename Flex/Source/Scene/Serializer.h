// Copyright (c) 2025 Evangelion Manuhutu

#ifndef SERIALIZER_H
#define SERIALIZER_H


#include "json.hpp"
#include "Scene.h"

#include <filesystem>

namespace nlohmann { using json = basic_json<>; }

namespace flex
{
	class SceneSerializer
	{
	public:
		SceneSerializer(const Ref<Scene>& scene);

		bool Serialize(const std::filesystem::path& filepath) const;
		bool Deserialize(const std::filesystem::path& filepath);

	private:
		void SerializeEntity(nlohmann::json& entities, entt::entity entity) const;
		void DeserializeEntity(const nlohmann::json& entityData);

		Ref<Scene> m_Scene;
	};
}

#endif