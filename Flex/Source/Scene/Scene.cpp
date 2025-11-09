// Copyright (c) 2025 Evangelion Manuhutu

#include "Scene.h"

#include "Renderer/Texture.h"
#include "Renderer/Material.h"

#include "Components.h"

namespace flex
{
	Scene::Scene()
	{
		registry = new entt::registry();
		joltPhysicsScene = JoltPhysicsScene::Create(this);
	}

	Scene::~Scene()
	{
		delete registry;
		registry = nullptr;
	}

    void Scene::Start()
    {
		m_IsPlaying = true;

		joltPhysicsScene->SimulationStart();
    }

    void Scene::Stop()
    {
		m_IsPlaying = false;
		joltPhysicsScene->SimulationStop();
    }

    void Scene::Update(float deltaTime)
    {
		if (m_IsPlaying)
		{
			joltPhysicsScene->Simulate(deltaTime);
		}
		else // Editor Update
		{
		}
    }

	void Scene::Render(const glm::mat4& viewProjection, const Ref<Texture2D> &environmentTexture)
	{
		auto view = registry->view<TransformComponent, MeshComponent>();
		view.each([this](TransformComponent &tr, MeshComponent &mc)
			{
				if (mc.meshInstance)
				{
					Ref<Material> mat = mc.meshInstance->material;
					if (mat)
					{
						mat->UpdateData();

						mat->occlusionTexture->Bind(4);
						mat->shader->SetUniform("u_OcclusionTexture", 4);

						mat->normalTexture->Bind(3);
						mat->shader->SetUniform("u_NormalTexture", 3);

						mat->metallicRoughnessTexture->Bind(2);
						mat->shader->SetUniform("u_MetallicRoughnessTexture", 2);

						mat->emissiveTexture->Bind(1);
						mat->shader->SetUniform("u_EmissiveTexture", 1);

						mat->baseColorTexture->Bind(0);
						mat->shader->SetUniform("u_BaseColorTexture", 0);
					}
				}
			});
	}

    entt::entity Scene::CreateEntity(const std::string &name, const UUID &uuid)
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
	entt::entity Scene::GetEntityByUUID(const UUID& uuid)
	{
		if (entities.contains(uuid))
		{
			return entities[uuid];
		}
		return entt::null;
	}
}