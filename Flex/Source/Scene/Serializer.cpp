// Copyright (c) 2025 Evangelion Manuhutu

#include "Serializer.h"
#include "Components.h"
#include "Renderer/Mesh.h"
#include "Renderer/Material.h"
#include "Math/Math.hpp"

#include <fstream>
#include <iomanip>

namespace flex
{
    using nlohmann::json;

    namespace
    {
        nlohmann::json SerializeVec2(const glm::vec2& v)
        {
            return nlohmann::json::array({ v.x, v.y });
        }

        glm::vec2 DeserializeVec2(const nlohmann::json& arr)
        {
            glm::vec2 result{ 0.0f };
            if (arr.is_array() && arr.size() == 2)
            {
                result.x = arr[0].get<float>();
                result.y = arr[1].get<float>();
            }
            return result;
        }

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

        nlohmann::json SerializeVec4(const glm::vec4& v)
        {
            return nlohmann::json::array({ v.x, v.y, v.z, v.w });
        }

        glm::vec4 DeserializeVec4(const nlohmann::json& arr)
        {
            glm::vec4 result{ 0.0f };
            if (arr.is_array() && arr.size() == 4)
            {
                result.x = arr[0].get<float>();
                result.y = arr[1].get<float>();
                result.z = arr[2].get<float>();
                result.w = arr[3].get<float>();
            }
            return result;
        }

        std::string ToMaterialTypeString(MaterialType type)
        {
            switch (type)
            {
            case MaterialType::Transparent: return "Transparent";
            case MaterialType::Opaque:
            default: return "Opaque";
            }
        }

        MaterialType MaterialTypeFromString(const std::string& typeStr)
        {
            if (typeStr == "Transparent")
            {
                return MaterialType::Transparent;
            }
            return MaterialType::Opaque;
        }

    }

    SceneSerializer::SceneSerializer(const Ref<Scene>& scene, SceneLoadOptions options)
        : m_Scene(scene), m_Options(options)
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
            if (mesh.meshInstance && mesh.meshInstance->material)
            {
                const Ref<Material>& material = mesh.meshInstance->material;
                json materialJson;
                materialJson["Name"] = material->name;
                materialJson["Type"] = ToMaterialTypeString(material->type);
                materialJson["BaseColorFactor"] = SerializeVec4(material->params.baseColorFactor);
                materialJson["EmissiveFactor"] = SerializeVec4(material->params.emissiveFactor);
                materialJson["MetallicFactor"] = material->params.metallicFactor;
                materialJson["RoughnessFactor"] = material->params.roughnessFactor;
                materialJson["OcclusionStrength"] = material->params.occlusionStrength;
                meshJson["Material"] = materialJson;
            }
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
            rbJson["Friction"] = rb.friction;
            rbJson["StaticFriction"] = rb.staticFriction;
            rbJson["Restitution"] = rb.restitution;
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
            boxJson["Offset"] = SerializeVec3(box.offset);
            boxJson["Density"] = box.density;
            boxJson["Scale"] = SerializeVec3(box.scale);
            entityJson["BoxCollider"] = boxJson;
        }

        if (m_Scene->HasComponent<CapsuleColliderComponent>(entity))
        {
            const CapsuleColliderComponent &capsule = m_Scene->GetComponent<CapsuleColliderComponent>(entity);
            json capsuleJson;
            capsuleJson["Offset"] = SerializeVec3(capsule.offset);
            capsuleJson["Density"] = capsule.density;
            capsuleJson["Radius"] = capsule.radius;
            capsuleJson["Height"] = capsule.height;
            entityJson["CapsuleCollider"] = capsuleJson;
        }

        if (m_Scene->HasComponent<SphereColliderComponent>(entity))
        {
            const SphereColliderComponent &sphere = m_Scene->GetComponent<SphereColliderComponent>(entity);
            json sphereJson;
            sphereJson["Offset"] = SerializeVec3(sphere.offset);
            sphereJson["Density"] = sphere.density;
            sphereJson["Radius"] = sphere.radius;
            entityJson["SphereCollider"] = sphereJson;
        }

        if (m_Scene->HasComponent<PlaneColliderComponent>(entity))
        {
            const PlaneColliderComponent& plane = m_Scene->GetComponent<PlaneColliderComponent>(entity);
            json planeJson;
            planeJson["Offset"] = SerializeVec3(plane.offset);
            entityJson["PlaneCollider"] = planeJson;
        }

        if (m_Scene->HasComponent<CameraComponent>(entity))
        {
            const CameraComponent& cam = m_Scene->GetComponent<CameraComponent>(entity);
            json camJson;
            camJson["Fov"] = cam.fov;
            camJson["NearPlane"] = cam.nearPlane;
            camJson["FarPlane"] = cam.farPlane;
            camJson["OrthoSize"] = cam.orthoSize;
            camJson["Primary"] = cam.primary;
            camJson["ProjectionType"] = static_cast<int>(cam.projectionType);
            camJson["AspectMode"] = static_cast<int>(cam.aspectMode);
            camJson["FixedAspectRatio"] = cam.fixedAspectRatio;
            camJson["ViewportSize"] = SerializeVec2(cam.viewportSize);
            entityJson["Camera"] = camJson;
        }

        if (m_Scene->HasComponent<AudioComponent>(entity))
        {
            const AudioComponent &audio = m_Scene->GetComponent<AudioComponent>(entity);
            json audioJson;
            audioJson["Name"] = audio.name;
            audioJson["SoundPath"] = audio.filepath;
            audioJson["Volume"] = audio.volume;
            audioJson["Panning"] = audio.panning;
            audioJson["Loop"] = audio.loop;
            audioJson["PlayOnStart"] = audio.playOnStart;
            
            entityJson["Audio"] = audioJson;
        }

        if (m_Scene->HasComponent<NativeScriptComponent>(entity))
        {
            json nscJson;
            nscJson["Enabled"] = true;
            // Note: We only serialize that the component exists.
            // The actual script binding must be done in code.
            entityJson["NativeScript"] = nscJson;
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

        if (m_Options.loadGraphics && entityData.contains("Mesh"))
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

                if (mesh.meshInstance && mesh.meshInstance->material && meshJson.contains("Material"))
                {
                    const json& materialJson = meshJson["Material"];
                    Ref<Material> material = mesh.meshInstance->material;
                    material->name = materialJson.value("Name", material->name);
                    material->type = MaterialTypeFromString(materialJson.value("Type", ToMaterialTypeString(material->type)));
                    material->params.baseColorFactor = DeserializeVec4(materialJson.value("BaseColorFactor", nlohmann::json::array()));
                    material->params.emissiveFactor = DeserializeVec4(materialJson.value("EmissiveFactor", nlohmann::json::array()));
                    material->params.metallicFactor = materialJson.value("MetallicFactor", material->params.metallicFactor);
                    material->params.roughnessFactor = materialJson.value("RoughnessFactor", material->params.roughnessFactor);
                    material->params.occlusionStrength = materialJson.value("OcclusionStrength", material->params.occlusionStrength);
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
            rb.friction = rbJson.value("Friction", 0.6f);
            rb.staticFriction = rbJson.value("StaticFriction", 0.6f);
            rb.restitution = rbJson.value("Restitution", 0.6f);
            rb.centerOfMass = DeserializeVec3(rbJson.value("CenterOfMass", json::array()));
            rb.MotionQuality = static_cast<RigidbodyComponent::EMotionQuality>(rbJson.value("MotionQuality", 0));
            rb.rotateX = rbJson.value("RotateX", true);
            rb.rotateY = rbJson.value("RotateY", true);
            rb.rotateZ = rbJson.value("RotateZ", true);
            rb.moveX = rbJson.value("MoveX", true);
            rb.moveY = rbJson.value("MoveY", true);
            rb.moveZ = rbJson.value("MoveZ", true);
            rb.bodyID = JPH::BodyID();
        }

        if (entityData.contains("BoxCollider"))
        {
            const json& boxJson = entityData["BoxCollider"];
            BoxColliderComponent& box = m_Scene->AddComponent<BoxColliderComponent>(entity);
            box.scale = DeserializeVec3(boxJson.value("Scale", json::array()));
            box.offset = DeserializeVec3(boxJson.value("Offset", json::array()));
            box.density = boxJson.value("Density", 1.0f);
            box.shape = nullptr;
        }

        if (entityData.contains("CapsuleCollider"))
        {
            const json& capsuleJson = entityData["CapsuleCollider"];
            CapsuleColliderComponent& capsule = m_Scene->AddComponent<CapsuleColliderComponent>(entity);
            capsule.offset = DeserializeVec3(capsuleJson.value("Offset", json::array()));
            capsule.density = capsuleJson.value("Density", 1.0f);
            capsule.radius = capsuleJson.value("Radius", 0.5f);
            capsule.height = capsuleJson.value("Height", 1.0f);
            capsule.shape = nullptr;
        }

        if (entityData.contains("SphereCollider"))
        {
            const json& sphereJson = entityData["SphereCollider"];
            SphereColliderComponent& sphere = m_Scene->AddComponent<SphereColliderComponent>(entity);
            sphere.offset = DeserializeVec3(sphereJson.value("Offset", json::array()));
            sphere.density = sphereJson.value("Density", 1.0f);
            sphere.radius = sphereJson.value("Radius", 0.5f);
            sphere.shape = nullptr;
        }

        if (entityData.contains("PlaneCollider"))
        {
            const json& planeJson = entityData["PlaneCollider"];
            PlaneColliderComponent& plane = m_Scene->AddComponent<PlaneColliderComponent>(entity);
            plane.offset = DeserializeVec3(planeJson.value("Offset", json::array()));
            plane.shape = nullptr;
        }

        if (entityData.contains("Camera"))
        {
            const json& camJson = entityData["Camera"];
            CameraComponent& cam = m_Scene->AddComponent<CameraComponent>(entity);
            cam.fov = camJson.value("Fov", 45.0f);
            cam.nearPlane = camJson.value("NearPlane", 0.1f);
            cam.farPlane = camJson.value("FarPlane", 550.0f);
            cam.orthoSize = camJson.value("OrthoSize", 10.0f);
            cam.primary = camJson.value("Primary", false);
            cam.projectionType = static_cast<ProjectionType>(camJson.value("ProjectionType", 0));
            cam.aspectMode = static_cast<CameraAspectMode>(camJson.value("AspectMode", static_cast<int>(cam.aspectMode)));
            cam.fixedAspectRatio = camJson.value("FixedAspectRatio", cam.fixedAspectRatio);
            if (camJson.contains("ViewportSize"))
            {
                cam.viewportSize = DeserializeVec2(camJson["ViewportSize"]);
            }

            cam.RecalculateProjection(m_Scene->GetViewportSize());
        }

        if (m_Options.loadAudio && entityData.contains("Audio"))
        {
            const json &audioJson = entityData["Audio"];
            AudioComponent &audio = m_Scene->AddComponent<AudioComponent>(entity);
            audio.name = audioJson.value("Name", "<EMPTY>");
            audio.filepath = audioJson.value("SoundPath", "<EMPTY>");
            audio.volume = audioJson.value("Volume", 1.0f);
            audio.panning = audioJson.value("Panning", 0.0f);
            audio.loop = audioJson.value("Loop", false);
            audio.playOnStart = audioJson.value("PlayOnStart", false);

            if (!audio.filepath.empty())
            {
                audio.sound = FmodSound::Create(audio.name, audio.filepath, audio.loop ? FMOD_LOOP_NORMAL : FMOD_INIT_NORMAL);
            }
        }

        if (entityData.contains("NativeScript"))
        {
            NativeScriptComponent& nsc = m_Scene->AddComponent<NativeScriptComponent>(entity);
            // Note: The script binding must be done in code after deserialization.
            // This just creates the component placeholder.
        }
    }

}