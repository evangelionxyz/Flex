// Copyright (c) 2025 Evangelion Manuhutu

#include "Serializer.h"
#include "Components.h"
#include "Renderer/Mesh.h"
#include "Math/Math.hpp"

#include <fstream>
#include <iomanip>

namespace flex
{
	using nlohmann::json;

	namespace
	{
		nlohmann::json SerializeVec3(const glm::vec3& v)
		{
			return nlohmann::json::array({ v.x, v.y, v.z });
		}

		glm::vec3 DeserializeVec3(const nlohmann::json& arr)
		{
			glm::vec3 result{ 0.0f };
			if (arr.is_array() && arr.size() == 3)
			{
				result.x = arr[0].get<float>();
				result.y = arr[1].get<float>();
				result.z = arr[2].get<float>();
			}
			return result;
		}

	}

	SceneSerializer::SceneSerializer(const Ref<Scene>& scene)
		: m_Scene(scene)
	{
	}

	bool SceneSerializer::Serialize(const std::filesystem::path& filepath) const
	{
		if (!m_Scene)
		{
			return false;
		}

		json sceneJson;
		sceneJson["SceneGravity"] = SerializeVec3(m_Scene->sceneGravity);

		json entities = json::array();
		for (const auto& [uuid, entity] : m_Scene->entities)
		{
			SerializeEntity(entities, entity);
		}

		sceneJson["Entities"] = entities;

		std::ofstream stream(filepath);
		if (!stream.is_open())
		{
			return false;
		}

		stream << std::setw(4) << sceneJson;
		return true;
	}

	bool SceneSerializer::Deserialize(const std::filesystem::path& filepath)
	{
		if (!m_Scene)
		{
			return false;
		}

		std::ifstream stream(filepath);
		if (!stream.is_open())
		{
			return false;
		}

		json sceneJson;
		stream >> sceneJson;

		m_Scene->registry->clear();
		m_Scene->entities.clear();
		m_Scene->sceneGravity = DeserializeVec3(sceneJson.value("SceneGravity", json::array()));

		const json& entities = sceneJson["Entities"];
		for (const auto& entityData : entities)
		{
			DeserializeEntity(entityData);
		}

		return true;
	}

	void SceneSerializer::SerializeEntity(json& entities, entt::entity entity) const
	{
		if (!m_Scene->IsValid(entity))
		{
			return;
		}

		json entityJson;

		const TagComponent& tag = m_Scene->GetComponent<TagComponent>(entity);
		entityJson["Entity"] = static_cast<uint64_t>(tag.uuid);

		json tagJson;
		tagJson["Name"] = tag.name;
		tagJson["Parent"] = static_cast<uint64_t>(tag.parent);
		json children = json::array();
		for (const UUID& childUUID : tag.children)
		{
			children.push_back(static_cast<uint64_t>(childUUID));
		}
		tagJson["Children"] = children;
		entityJson["Tag"] = tagJson;

		if (m_Scene->HasComponent<TransformComponent>(entity))
		{
			const TransformComponent& transform = m_Scene->GetComponent<TransformComponent>(entity);
			json transformJson;
			transformJson["Position"] = SerializeVec3(transform.position);
			transformJson["Rotation"] = SerializeVec3(transform.rotation);
			transformJson["Scale"] = SerializeVec3(transform.scale);
			entityJson["Transform"] = transformJson;
		}

		if (m_Scene->HasComponent<MeshComponent>(entity))
		{
			const MeshComponent& mesh = m_Scene->GetComponent<MeshComponent>(entity);
			json meshJson;
			meshJson["MeshPath"] = mesh.meshPath;
			meshJson["MeshIndex"] = mesh.meshIndex;
			entityJson["Mesh"] = meshJson;
		}

		if (m_Scene->HasComponent<RigidbodyComponent>(entity))
		{
			const RigidbodyComponent& rb = m_Scene->GetComponent<RigidbodyComponent>(entity);
			json rbJson;
			rbJson["UseGravity"] = rb.useGravity;
			rbJson["IsStatic"] = rb.isStatic;
			rbJson["Mass"] = rb.mass;
			rbJson["AllowSleeping"] = rb.allowSleeping;
			rbJson["RetainAcceleration"] = rb.retainAcceleration;
			rbJson["GravityFactor"] = rb.gravityFactor;
			rbJson["CenterOfMass"] = SerializeVec3(rb.centerOfMass);
			rbJson["MotionQuality"] = static_cast<int>(rb.MotionQuality);
			rbJson["RotateX"] = rb.rotateX;
			rbJson["RotateY"] = rb.rotateY;
			rbJson["RotateZ"] = rb.rotateZ;
			rbJson["MoveX"] = rb.moveX;
			rbJson["MoveY"] = rb.moveY;
			rbJson["MoveZ"] = rb.moveZ;
			entityJson["Rigidbody"] = rbJson;
		}

		if (m_Scene->HasComponent<BoxColliderComponent>(entity))
		{
			const BoxColliderComponent& box = m_Scene->GetComponent<BoxColliderComponent>(entity);
			json boxJson;
			boxJson["Scale"] = SerializeVec3(box.scale);
			boxJson["Offset"] = SerializeVec3(box.offset);
			boxJson["Friction"] = box.friction;
			boxJson["StaticFriction"] = box.staticFriction;
			boxJson["Restitution"] = box.restitution;
			boxJson["Density"] = box.density;
			entityJson["BoxCollider"] = boxJson;
		}

		entities.push_back(entityJson);
	}

	void SceneSerializer::DeserializeEntity(const json& entityData)
	{
		uint64_t entityID = entityData["Entity"].get<uint64_t>();
		const json& tagJson = entityData["Tag"];
		std::string name = tagJson.value("Name", std::string("Entity"));

		entt::entity entity = m_Scene->CreateEntity(name, UUID(entityID));
		TagComponent& tag = m_Scene->GetComponent<TagComponent>(entity);
		tag.parent = UUID(tagJson.value<uint64_t>("Parent", 0));
		tag.children.clear();
		if (tagJson.contains("Children"))
		{
			for (const auto& child : tagJson["Children"])
			{
				tag.children.insert(UUID(child.get<uint64_t>()));
			}
		}

		if (entityData.contains("Transform"))
		{
			TransformComponent& transform = m_Scene->AddComponent<TransformComponent>(entity);
			const json& transformJson = entityData["Transform"];
			transform.position = DeserializeVec3(transformJson.value("Position", json::array()));
			transform.rotation = DeserializeVec3(transformJson.value("Rotation", json::array()));
			transform.scale = DeserializeVec3(transformJson.value("Scale", json::array()));
		}

		if (entityData.contains("Mesh"))
		{
			const json& meshJson = entityData["Mesh"];
			std::string meshPath = meshJson.value("MeshPath", std::string());
			if (!meshPath.empty())
			{
				MeshComponent& mesh = m_Scene->AddComponent<MeshComponent>(entity);
				mesh.meshPath = meshPath;
				mesh.meshIndex = meshJson.value("MeshIndex", -1);

				MeshScene meshScene = MeshLoader::LoadSceneGraphFromGLTF(meshPath);
				if (!meshScene.flatMeshes.empty())
				{
					if (mesh.meshIndex >= 0 && mesh.meshIndex < static_cast<int>(meshScene.flatMeshes.size()))
					{
						mesh.meshInstance = meshScene.flatMeshes[mesh.meshIndex];
					}
					else
					{
						mesh.meshInstance = meshScene.flatMeshes.front();
						mesh.meshIndex = mesh.meshInstance ? mesh.meshInstance->meshIndex : -1;
					}
				}

				if (m_Scene->HasComponent<TransformComponent>(entity) && mesh.meshInstance)
				{
					const auto& transform = m_Scene->GetComponent<TransformComponent>(entity);
					mesh.meshInstance->worldTransform = math::ComposeTransform(transform);
				}
			}
		}

		if (entityData.contains("Rigidbody"))
		{
			const json& rbJson = entityData["Rigidbody"];
			RigidbodyComponent& rb = m_Scene->AddComponent<RigidbodyComponent>(entity);
			rb.useGravity = rbJson.value("UseGravity", true);
			rb.isStatic = rbJson.value("IsStatic", false);
			rb.mass = rbJson.value("Mass", 1.0f);
			rb.allowSleeping = rbJson.value("AllowSleeping", true);
			rb.retainAcceleration = rbJson.value("RetainAcceleration", false);
			rb.gravityFactor = rbJson.value("GravityFactor", 1.0f);
			rb.centerOfMass = DeserializeVec3(rbJson.value("CenterOfMass", json::array()));
			rb.MotionQuality = static_cast<RigidbodyComponent::EMotionQuality>(rbJson.value("MotionQuality", 0));
			rb.rotateX = rbJson.value("RotateX", true);
			rb.rotateY = rbJson.value("RotateY", true);
			rb.rotateZ = rbJson.value("RotateZ", true);
			rb.moveX = rbJson.value("MoveX", true);
			rb.moveY = rbJson.value("MoveY", true);
			rb.moveZ = rbJson.value("MoveZ", true);
			rb.body = nullptr;
		}

		if (entityData.contains("BoxCollider"))
		{
			const json& boxJson = entityData["BoxCollider"];
			BoxColliderComponent& box = m_Scene->AddComponent<BoxColliderComponent>(entity);
			box.scale = DeserializeVec3(boxJson.value("Scale", json::array()));
			box.offset = DeserializeVec3(boxJson.value("Offset", json::array()));
			box.friction = boxJson.value("Friction", 0.6f);
			box.staticFriction = boxJson.value("StaticFriction", 0.6f);
			box.restitution = boxJson.value("Restitution", 0.6f);
			box.density = boxJson.value("Density", 1.0f);
			box.shape = nullptr;
		}
	}

}