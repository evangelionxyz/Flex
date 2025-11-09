// Copyright (c) 2025 Evangelion Manuhutu

#include "Scene.h"
#include "Components.h"

#include "Renderer/Texture.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/Renderer.h"
#include "Renderer/Renderer2D.h"
#include "Math/Math.hpp"

#include <filesystem>
#include <unordered_map>
#include <type_traits>
#include <format>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace flex
{
	namespace detail
	{
		template<typename Component>
		Component PrepareComponentCopy(const Component& component)
		{
			if constexpr (std::is_same_v<Component, RigidbodyComponent>)
			{
				Component copy = component;
				copy.bodyID = JPH::BodyID();
				return copy;
			}
			else if constexpr (std::is_same_v<Component, BoxColliderComponent>)
			{
				Component copy = component;
				copy.shape = nullptr;
				return copy;
			}
			else
			{
				return component;
			}
		}

		template<typename... Component>
		struct ComponentGroup
		{
		};

		template<typename Component>
		bool CopyComponentToEntity(const Scene& source, entt::entity sourceEntity, Scene& destination, entt::entity destinationEntity)
		{
			if (!source.registry->all_of<Component>(sourceEntity))
			{
				return false;
			}
			const Component& srcComponent = source.registry->get<Component>(sourceEntity);
			Component componentCopy = PrepareComponentCopy(srcComponent);
			if (destination.HasComponent<Component>(destinationEntity))
			{
				destination.GetComponent<Component>(destinationEntity) = componentCopy;
			}
			else
			{
				destination.AddComponent<Component>(destinationEntity, componentCopy);
			}
			return true;
		}

		template<typename... Component>
		bool CopyComponentToEntityGroup(const Scene& source, entt::entity sourceEntity, Scene& destination, entt::entity destinationEntity, ComponentGroup<Component...>)
		{
			bool copiedAny = false;
			((copiedAny = CopyComponentToEntity<Component>(source, sourceEntity, destination, destinationEntity) || copiedAny), ...);
			return copiedAny;
		}

		template<typename Component>
		void CopyComponent(const Scene& source, const Ref<Scene>& destination)
		{
			auto view = source.registry->view<Component>();
			view.each([&](entt::entity entity, const Component& component)
			{
				const UUID uuid = source.registry->get<TagComponent>(entity).uuid;
				entt::entity clonedEntity = destination->GetEntityByUUID(uuid);
				if (clonedEntity == entt::null)
				{
					return;
				}

				Component componentCopy = PrepareComponentCopy(component);
				if (destination->HasComponent<Component>(clonedEntity))
				{
					destination->GetComponent<Component>(clonedEntity) = componentCopy;
				}
				else
				{
					destination->AddComponent<Component>(clonedEntity, componentCopy);
				}
			});
		}

		template<typename... Component>
		void CopyComponentGroup(const Scene& source, const Ref<Scene>& destination, ComponentGroup<Component...>)
		{
			(CopyComponent<Component>(source, destination), ...);
		}

		using AllComponents = ComponentGroup<TransformComponent, MeshComponent, RigidbodyComponent, BoxColliderComponent>;
	}

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

	void Scene::DebugDrawColliders() const
	{
		if (!registry)
		{
			return;
		}

		auto view = registry->view<TransformComponent, BoxColliderComponent>();
		constexpr glm::vec3 kLocalCorners[8] = {
			{ -0.5f, -0.5f, -0.5f },
			{  0.5f, -0.5f, -0.5f },
			{  0.5f,  0.5f, -0.5f },
			{ -0.5f,  0.5f, -0.5f },
			{ -0.5f, -0.5f,  0.5f },
			{  0.5f, -0.5f,  0.5f },
			{  0.5f,  0.5f,  0.5f },
			{ -0.5f,  0.5f,  0.5f }
		};

		constexpr uint32_t kEdgeIndices[12][2] = {
			{0, 1}, {1, 2}, {2, 3}, {3, 0},
			{4, 5}, {5, 6}, {6, 7}, {7, 4},
			{0, 4}, {1, 5}, {2, 6}, {3, 7}
		};

		const glm::vec4 kDebugColor = { 0.9f, 0.0f, 0.9f, 1.0f };

		view.each([&](const TransformComponent& transform, const BoxColliderComponent& box)
		{
			const glm::quat rotation = glm::quat(glm::radians(transform.rotation));
			const glm::vec3 worldOffset = rotation * (box.offset * transform.scale);
			const glm::vec3 worldScale = transform.scale * box.scale * 2.0f;
			const glm::mat4 colliderTransform = glm::translate(glm::mat4(1.0f), transform.position + worldOffset)
				* glm::toMat4(rotation)
				* glm::scale(glm::mat4(1.0f), worldScale);

			glm::vec3 worldCorners[8];
			for (size_t i = 0; i < 8; ++i)
			{
				worldCorners[i] = glm::vec3(colliderTransform * glm::vec4(kLocalCorners[i], 1.0f));
			}

			for (const auto& edge : kEdgeIndices)
			{
				Renderer2D::DrawLine(worldCorners[edge[0]], worldCorners[edge[1]], kDebugColor);
			}
		});
	}

	std::vector<entt::entity> Scene::LoadModel(const std::string& filepath, const glm::mat4& rootTransform)
	{
		MeshScene meshScene = MeshLoader::LoadSceneGraphFromGLTF(filepath);
		std::vector<entt::entity> createdEntities;
		createdEntities.reserve(meshScene.flatMeshes.size());

		if (meshScene.nodes.empty())
		{
			return createdEntities;
		}

		std::string fallbackName = std::filesystem::path(filepath).stem().string();
		if (fallbackName.empty())
		{
			fallbackName = "Mesh";
		}


		std::unordered_map<std::string, std::size_t> nameUsage;

		for (const MeshNode& node : meshScene.nodes)
		{
			if (node.meshInstances.empty())
			{
				continue;
			}

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
				meshComponent.meshIndex = meshInstance ? meshInstance->meshIndex : -1;
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

    entt::entity Scene::DuplicateEntity(entt::entity entity)
    {
		if (!IsValid(entity))
		{
			return entt::null;
		}

		const TagComponent& sourceTag = GetComponent<TagComponent>(entity);

		std::string baseName = sourceTag.name;
		if (baseName.empty())
		{
			baseName = "Entity";
		}

		std::string duplicateName = baseName;
		int suffix = 1;
		while (true)
		{
			bool exists = false;
			for (const auto& [uuid, existingEntity] : entities)
			{
				if (GetComponent<TagComponent>(existingEntity).name == duplicateName)
				{
					exists = true;
					break;
				}
			}

			if (!exists)
			{
				break;
			}

			duplicateName = std::format("{} ({})", baseName, suffix);
			++suffix;
		}

		entt::entity duplicateEntity = CreateEntity(duplicateName);
		TagComponent& duplicateTag = GetComponent<TagComponent>(duplicateEntity);
		duplicateTag.parent = UUID(0);
		duplicateTag.children.clear();

		detail::CopyComponentToEntityGroup(*this, entity, *this, duplicateEntity, detail::AllComponents{});

		return duplicateEntity;
    }

    Ref<Scene> Scene::Clone() const
	{
		Ref<Scene> clonedScene = CreateRef<Scene>();
		clonedScene->sceneGravity = sceneGravity;

		for (const auto& [uuid, entity] : entities)
		{
			const TagComponent& sourceTag = registry->get<TagComponent>(entity);
			entt::entity clonedEntity = clonedScene->CreateEntity(sourceTag.name, uuid);
			TagComponent& clonedTag = clonedScene->GetComponent<TagComponent>(clonedEntity);
			clonedTag.parent = sourceTag.parent;
			clonedTag.children = sourceTag.children;
		}

		detail::CopyComponentGroup(*this, clonedScene, detail::AllComponents{});

		return clonedScene;
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