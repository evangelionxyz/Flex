// Copyright (c) 2025 Evangelion Manuhutu

#include "Scene.h"

#include "Renderer/Texture.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/Renderer.h"
#include "Math/Math.hpp"

#include <filesystem>
#include <unordered_map>

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

	void Scene::Render(const Ref<Shader>& shader, const Ref<Texture2D>& environmentTexture)
	{
		if (!shader)
			return;

		auto view = registry->view<TransformComponent, MeshComponent>();
		view.each([&](TransformComponent& transform, MeshComponent& meshComponent)
			{
				if (!meshComponent.meshInstance || !meshComponent.meshInstance->mesh)
					return;

				const glm::mat4 worldTransform = math::ComposeTransform(transform);

				const Ref<Material>& material = meshComponent.meshInstance->material;
				if (material)
				{
					material->UpdateData();

					material->occlusionTexture->Bind(4);
					shader->SetUniform("u_OcclusionTexture", 4);

					material->normalTexture->Bind(3);
					shader->SetUniform("u_NormalTexture", 3);

					material->metallicRoughnessTexture->Bind(2);
					shader->SetUniform("u_MetallicRoughnessTexture", 2);

					material->emissiveTexture->Bind(1);
					shader->SetUniform("u_EmissiveTexture", 1);

					material->baseColorTexture->Bind(0);
					shader->SetUniform("u_BaseColorTexture", 0);
				}

				if (environmentTexture)
				{
					environmentTexture->Bind(5);
					shader->SetUniform("u_EnvironmentTexture", 5);
				}

				shader->SetUniform("u_Transform", worldTransform);

				meshComponent.meshInstance->mesh->vertexArray->Bind();
				Renderer::DrawIndexed(meshComponent.meshInstance->mesh->vertexArray);
			});
	}

	void Scene::RenderDepth(const Ref<Shader>& shader)
	{
		if (!shader)
			return;

		auto view = registry->view<TransformComponent, MeshComponent>();
		view.each([&](TransformComponent& transform, MeshComponent& meshComponent)
			{
				if (!meshComponent.meshInstance || !meshComponent.meshInstance->mesh)
					return;

				const glm::mat4 worldTransform = math::ComposeTransform(transform);
				shader->SetUniform("u_Model", worldTransform);
				meshComponent.meshInstance->mesh->vertexArray->Bind();
				Renderer::DrawIndexed(meshComponent.meshInstance->mesh->vertexArray);
			});
	}

	std::vector<entt::entity> Scene::LoadModel(const std::string& filepath, const glm::mat4& rootTransform)
	{
		MeshScene meshScene = MeshLoader::LoadSceneGraphFromGLTF(filepath);
		std::vector<entt::entity> createdEntities;
		createdEntities.reserve(meshScene.flatMeshes.size());

		if (meshScene.nodes.empty())
			return createdEntities;

		std::string fallbackName = std::filesystem::path(filepath).stem().string();
		if (fallbackName.empty())
			fallbackName = "Mesh";

		std::unordered_map<std::string, std::size_t> nameUsage;

		for (const MeshNode& node : meshScene.nodes)
		{
			if (node.meshInstances.empty())
				continue;

			std::size_t primitiveIndex = 0;
			for (const Ref<MeshInstance>& meshInstance : node.meshInstances)
			{
				if (!meshInstance || !meshInstance->mesh)
				{
					++primitiveIndex;
					continue;
				}

				std::string baseName = node.name.empty() ? fallbackName : node.name;
				if (node.meshInstances.size() > 1)
				{
					baseName += "_" + std::to_string(primitiveIndex);
				}

				auto& usage = nameUsage[baseName];
				std::string finalName = baseName;
				if (usage > 0)
				{
					finalName += "_" + std::to_string(usage);
				}
				++usage;

				entt::entity entity = CreateEntity(finalName);
				TagComponent& tag = GetComponent<TagComponent>(entity);
				tag.scene = this;

				TransformComponent& transform = AddComponent<TransformComponent>(entity);
				const glm::mat4 worldMatrix = rootTransform * meshInstance->worldTransform;
				math::DecomposeTransform(worldMatrix, transform);

				MeshComponent& meshComponent = AddComponent<MeshComponent>(entity);
				meshComponent.meshPath = filepath;
				meshComponent.meshInstance = meshInstance;
				meshInstance->worldTransform = worldMatrix;

				createdEntities.push_back(entity);
				++primitiveIndex;
			}
		}

		return createdEntities;
	}

    entt::entity Scene::CreateEntity(const std::string &name, const UUID &uuid)
    {
		assert(registry && "Registry is null!");
		entt::entity newEntity = registry->create();
		TagComponent& tag = AddComponent<TagComponent>(newEntity, name, uuid);
		tag.scene = this;

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