// Copyright (c) 2025 Evangelion Manuhutu

#include "Scene.h"

namespace flex
{
	Scene::Scene()
	{
		registry = new entt::registry();
	}

	Scene::~Scene()
	{
		delete registry;
		registry = nullptr;
	}

	entt::entity Scene::CreateEntity(const std::string &name, const UUID& uuid)
	{
		assert(registry && "Registry is null!");
		entt::entity newEntity = registry->create();
		AddComponent<TagComponent>(newEntity, name, uuid);

		entities[uuid] = newEntity;
		return newEntity;
	}

	void Scene::DestroyEntity(const entt::entity entity)
	{
		assert(registry && "Registry is null!");
		if (registry->valid(entity))
		{
			TagComponent& tag = GetComponent<TagComponent>(entity);
			registry->destroy(entity);
			entities.erase(tag.uuid);
		}
	}
}