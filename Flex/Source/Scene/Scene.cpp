// Copyright (c) 2025 Evangelion Manuhutu

#include "Scene.h"
#include "Components.h"

#include "Physics/JoltPhysicsScene.h"

#include "Renderer/Texture.h"
#include "Renderer/Material.h"
#include "Renderer/Mesh.h"
#include "Renderer/Renderer.h"
#include "Renderer/Renderer2D.h"
#include "Math/Math.hpp"

#include "ScriptableEntity.h"
#include "GameTest/PlayerController.h"

#include <filesystem>
#include <unordered_map>
#include <type_traits>
#include <format>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
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

        using AllComponents = ComponentGroup<TransformComponent,
            MeshComponent,
            RigidbodyComponent,
            BoxColliderComponent,
            CapsuleColliderComponent,
            SphereColliderComponent,
            PlaneColliderComponent,
            NativeScriptComponent,
            AudioComponent,
            CameraComponent>;
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

        registry->view<AudioComponent>().each([](entt::entity entity, AudioComponent &audio)
        {
            if (audio.sound && audio.playOnStart)
            {
                audio.sound->Stop();
                audio.sound->Play();

                audio.sound->SetVolume(audio.volume);
                audio.sound->SetPan(audio.panning);
            }
        });

        auto nscView = registry->view<NativeScriptComponent>();
        nscView.each([&](entt::entity entity, NativeScriptComponent &nsc)
        {
            if (!nsc.instance && nsc.InstantiateScript)
            {
                nsc.instance = nsc.InstantiateScript(this, entity);
                nsc.instance->OnStart();
            }
        });

        joltPhysicsScene->SimulationStart();
    }

    void Scene::Stop()
    {
        m_IsPlaying = false;

        registry->view<AudioComponent>().each([](entt::entity entity, AudioComponent &audio)
        {
            if (audio.sound)
            {
                audio.sound->Stop();
            }
        });

        auto nscView = registry->view<NativeScriptComponent>();
        nscView.each([&](entt::entity entity, NativeScriptComponent &nsc)
        {
            if (nsc.DestroyScript)
            {
                nsc.instance->OnStop();
                nsc.DestroyScript(&nsc);
            }
        });

        joltPhysicsScene->SimulationStop();
    }

    void Scene::Update(float deltaTime)
    {
        if (m_IsPlaying)
        {
            auto nscView = registry->view<NativeScriptComponent>();
            nscView.each([&](entt::entity entity, NativeScriptComponent &nsc)
            {
                if (nsc.instance)
                {
                    nsc.instance->OnUpdate(deltaTime);
                }
            });

            joltPhysicsScene->Simulate(deltaTime);
        }
        else // Editor Update
        {
        }

        if (!registry)
        {
            return;
        }

        auto cameraView = registry->view<TransformComponent, CameraComponent>();
        cameraView.each([&](entt::entity entity, const TransformComponent&, CameraComponent& camera)
        {
            const glm::mat4 worldTransform = GetWorldTransform(entity);
            camera.view = glm::inverse(worldTransform);
        });
    }

    void Scene::OnMouseMotion(const glm::vec2& delta)
    {
        if (!m_IsPlaying || !registry)
            return;

        auto nscView = registry->view<NativeScriptComponent>();
        nscView.each([&](entt::entity entity, NativeScriptComponent &nsc)
        {
            if (nsc.instance)
            {
                // Check if the script has OnMouseMotion method
                auto* playerController = dynamic_cast<PlayerController*>(nsc.instance);
                if (playerController)
                {
                    playerController->OnMouseMotion(delta);
                }
            }
        });
    }

    void Scene::ResizeViewport(const glm::vec2& size)
    {
        m_Viewport = size;

        if (!registry || size.x <= 0.0f || size.y <= 0.0f)
        {
            return;
        }

        auto cameraView = registry->view<CameraComponent>();
        cameraView.each([&](CameraComponent& camera)
            {
                camera.RecalculateProjection(size);
            });
    }

    glm::vec2 Scene::GetViewportSize() const
    {
        return m_Viewport;
    }

    void Scene::Render(const Ref<Shader>& shader, const Ref<Texture2D>& environmentTexture)
    {
        if (!shader)
            return;

        auto view = registry->view<TransformComponent, MeshComponent>();
        view.each([&](entt::entity entity, TransformComponent&, MeshComponent& meshComponent)
            {
                if (!meshComponent.meshInstance || !meshComponent.meshInstance->mesh)
                    return;

                const glm::mat4 worldTransform = GetWorldTransform(entity);
                if (meshComponent.meshInstance)
                {
                    meshComponent.meshInstance->worldTransform = worldTransform;
                }

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
        view.each([&](entt::entity entity, TransformComponent&, MeshComponent& meshComponent)
            {
                if (!meshComponent.meshInstance || !meshComponent.meshInstance->mesh)
                    return;

                const glm::mat4 worldTransform = GetWorldTransform(entity);
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

        view.each([&](entt::entity entity, const TransformComponent&, const BoxColliderComponent& box)
        {
            TransformComponent worldTransform;
            math::DecomposeTransform(GetWorldTransform(entity), worldTransform);
            const glm::quat rotation = glm::quat(glm::radians(worldTransform.rotation));
            const glm::vec3 worldOffset = rotation * (box.offset * worldTransform.scale);
            const glm::vec3 worldScale = worldTransform.scale * box.scale * 2.0f;
            const glm::mat4 colliderTransform = glm::translate(glm::mat4(1.0f), worldTransform.position + worldOffset)
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

    std::vector<entt::entity> Scene::LoadModel(const std::string& filepath, const glm::mat4& rootTransform, entt::entity rootEntity)
    {
        MeshScene meshScene = MeshLoader::LoadSceneGraphFromGLTF(filepath);
        std::vector<entt::entity> createdEntities;
        createdEntities.reserve(meshScene.flatMeshes.size() + 1);

        if (meshScene.nodes.empty())
        {
            return createdEntities;
        }

        std::string fallbackName = std::filesystem::path(filepath).stem().string();
        if (fallbackName.empty())
        {
            fallbackName = "Mesh";
        }

        auto makeUniqueName = [&](const std::string& baseName)
        {
            if (baseName.empty())
            {
                return std::string("Mesh");
            }

            std::string candidate = baseName;
            int suffix = 1;
            while (GetEntityByName(candidate) != entt::null)
            {
                candidate = std::format("{} ({})", baseName, suffix++);
            }
            return candidate;
        };

        const bool usingExistingRoot = rootEntity != entt::null && registry && registry->valid(rootEntity) && HasComponent<TagComponent>(rootEntity);
        entt::entity groupEntity = entt::null;
        bool applyRootMatrix = false;

        if (usingExistingRoot)
        {
            groupEntity = rootEntity;
        }
        else
        {
            const std::string groupName = makeUniqueName(std::format("{} Scene", fallbackName));
            groupEntity = CreateEntity(groupName);
            applyRootMatrix = true;
        }

        TagComponent& groupTag = GetComponent<TagComponent>(groupEntity);
        groupTag.scene = this;

        const bool hasTransform = HasComponent<TransformComponent>(groupEntity);
        TransformComponent& groupTransform = hasTransform
            ? GetComponent<TransformComponent>(groupEntity)
            : AddComponent<TransformComponent>(groupEntity);

        if (!hasTransform || applyRootMatrix)
        {
            math::DecomposeTransform(rootTransform, groupTransform);
        }

        const glm::mat4 groupMatrix = math::ComposeTransform(groupTransform);
        glm::mat4 inverseGroupMatrix = glm::inverse(groupMatrix);
        createdEntities.push_back(groupEntity);

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
                const glm::mat4 localMatrix = inverseGroupMatrix * worldMatrix;
                math::DecomposeTransform(localMatrix, transform);

                MeshComponent& meshComponent = AddComponent<MeshComponent>(entity);
                meshComponent.meshPath = filepath;
                meshComponent.meshInstance = meshInstance;
                meshComponent.meshIndex = meshInstance ? meshInstance->meshIndex : -1;
                meshInstance->worldTransform = worldMatrix;

                ReparentEntity(entity, groupEntity);
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
            if (entt::entity parentEntity = GetParentEntity(entity); parentEntity != entt::null)
            {
                TagComponent& parentTag = GetComponent<TagComponent>(parentEntity);
                parentTag.children.erase(tag.uuid);
            }

            std::vector<UUID> childrenToDetach(tag.children.begin(), tag.children.end());
            for (const UUID& childUUID : childrenToDetach)
            {
                entt::entity childEntity = GetEntityByUUID(childUUID);
                if (childEntity != entt::null)
                {
                    TagComponent& childTag = GetComponent<TagComponent>(childEntity);
                    childTag.parent = UUID(0);
                }
            }
            tag.children.clear();

            registry->destroy(entity);
            entities.erase(tag.uuid);
        }
    }

    entt::entity Scene::GetEntityByUUID(const UUID& uuid) const
    {
        const auto it = entities.find(uuid);
        if (it != entities.end())
        {
            return it->second;
        }
        return entt::null;
    }

    entt::entity Scene::GetEntityByName(const std::string &name)
    {
        auto it = std::find_if(entities.begin(), entities.end(), [&](std::pair<UUID, entt::entity> pair)
        {
            if (this->HasComponent<TagComponent>(pair.second))
            {
                const auto &tag = this->GetComponent<TagComponent>(pair.second);
                return tag.name == name;
            }
            return false;
        });

        if (it != entities.end())
        {
            return it->second;
        }

        return entt::null;
    }

    entt::entity Scene::GetPrimaryCamera()
    {
        auto camView = registry->view<CameraComponent>();
        for (entt::entity e : camView)
        {
            CameraComponent &cam = registry->get<CameraComponent>(e);
            if (cam.primary)
            {
                return e;
            }
        }
        return entt::null;
    }

    void Scene::SetPrimaryCamera(entt::entity entity)
    {
        if (!registry)
        {
            return;
        }

        auto cameraView = registry->view<CameraComponent>();
        cameraView.each([&](entt::entity e, CameraComponent& camera)
            {
                camera.primary = (e == entity);
            });
    }

    const std::string &Scene::GetEntityName(entt::entity entity)
    {
        return GetComponent<TagComponent>(entity).name;
    }

    const UUID Scene::GetEntityUUID(entt::entity entity)
    {
        return GetComponent<TagComponent>(entity).uuid;
    }

    entt::entity Scene::GetParentEntity(entt::entity entity) const
    {
        if (!registry || !registry->valid(entity) || !registry->all_of<TagComponent>(entity))
        {
            return entt::null;
        }

        const TagComponent& tag = registry->get<TagComponent>(entity);
        if (static_cast<uint64_t>(tag.parent) == 0)
        {
            return entt::null;
        }

        const auto it = entities.find(tag.parent);
        if (it == entities.end())
        {
            return entt::null;
        }
        return it->second;
    }

    bool Scene::ReparentEntity(entt::entity child, entt::entity newParent)
    {
        if (!registry || !registry->valid(child))
        {
            return false;
        }

        if (newParent != entt::null && !registry->valid(newParent))
        {
            return false;
        }

        if (child == newParent)
        {
            return false;
        }

        if (newParent != entt::null && IsDescendant(child, newParent))
        {
            return false;
        }

        TagComponent& childTag = GetComponent<TagComponent>(child);
        entt::entity currentParent = GetParentEntity(child);
        if (currentParent == newParent)
        {
            return false;
        }

        if (currentParent != entt::null)
        {
            TagComponent& parentTag = GetComponent<TagComponent>(currentParent);
            parentTag.children.erase(childTag.uuid);
        }

        childTag.parent = UUID(0);

        if (newParent != entt::null)
        {
            TagComponent& newParentTag = GetComponent<TagComponent>(newParent);
            newParentTag.children.insert(childTag.uuid);
            childTag.parent = newParentTag.uuid;
        }

        return true;
    }

    bool Scene::IsDescendant(entt::entity ancestor, entt::entity entity) const
    {
        if (ancestor == entt::null || entity == entt::null)
        {
            return false;
        }

        entt::entity current = GetParentEntity(entity);
        constexpr int kMaxDepth = 1024;
        int depth = 0;
        while (current != entt::null && depth++ < kMaxDepth)
        {
            if (current == ancestor)
            {
                return true;
            }
            current = GetParentEntity(current);
        }

        return false;
    }

    glm::mat4 Scene::GetWorldTransform(entt::entity entity) const
    {
        if (!registry || !registry->valid(entity) || !registry->all_of<TransformComponent>(entity))
        {
            return glm::mat4(1.0f);
        }

        glm::mat4 world = math::ComposeTransform(registry->get<TransformComponent>(entity));
        entt::entity parent = GetParentEntity(entity);
        constexpr int kMaxDepth = 1024;
        int depth = 0;
        while (parent != entt::null && depth++ < kMaxDepth)
        {
            if (!registry->all_of<TransformComponent>(parent))
            {
                parent = GetParentEntity(parent);
                continue;
            }

            const glm::mat4 parentWorld = math::ComposeTransform(registry->get<TransformComponent>(parent));
            world = parentWorld * world;
            parent = GetParentEntity(parent);
        }

        return world;
    }

    void Scene::SetWorldTransform(entt::entity entity, const glm::mat4& worldTransform)
    {
        if (!registry || !registry->valid(entity) || !registry->all_of<TransformComponent>(entity))
        {
            return;
        }

        glm::mat4 local = worldTransform;
        if (entt::entity parent = GetParentEntity(entity); parent != entt::null && registry->all_of<TransformComponent>(parent))
        {
            const glm::mat4 parentWorld = GetWorldTransform(parent);
            const glm::mat4 parentInverse = glm::inverse(parentWorld);
            local = parentInverse * worldTransform;
        }

        TransformComponent& transform = registry->get<TransformComponent>(entity);
        math::DecomposeTransform(local, transform);
    }
}