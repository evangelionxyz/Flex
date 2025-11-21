// Copyright (c) 2025 Evangelion Manuhutu

#ifndef SCENE_H
#define SCENE_H

#include "entt/entt.hpp"
#include "Core/UUID.h"

#include "Physics/JoltPhysics.h"
#include "Physics/JoltPhysicsScene.h"

#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace flex
{
    class Texture2D;
    class Shader;

    struct CameraComponent;

    class Scene
    {
    public:
        Scene();
        ~Scene();

        void Start();
        void Stop();
        void Update(float deltaTime);
        void OnMouseMotion(const glm::vec2& delta);

        void ResizeViewport(const glm::vec2 &size);
        glm::vec2 GetViewportSize() const;

        void Render(const Ref<Shader>& shader, const Ref<Texture2D>& environmentTexture);
        void RenderDepth(const Ref<Shader>& shader);
        void DebugDrawColliders() const;

        bool IsPlaying() const { return m_IsPlaying; }

        std::vector<entt::entity> LoadModel(const std::string& filepath,
            const glm::mat4& rootTransform = glm::mat4(1.0f),
            entt::entity rootEntity = entt::null);

        entt::entity CreateEntity(const std::string& name, const UUID &uuid = UUID());
        entt::entity DuplicateEntity(entt::entity entity);
        void DestroyEntity(const entt::entity entity);

        Ref<Scene> Clone() const;

        template<typename T, typename... Args>
        T& AddComponent(entt::entity entity, Args &&... args)
        {
            if (HasComponent<T>(entity))
            {
                return GetComponent<T>(entity);
            }

            T& comp = registry->emplace<T>(entity, std::forward<Args>(args)...);
            return comp;
        }

        template<typename T>
        bool RemoveComponent(entt::entity entity)
        {
            if (!HasComponent<T>(entity))
                return false;
            
            registry->remove<T>(entity);
            return true;
        }

        template<typename T>
        T& GetComponent(entt::entity entity)
        {
            return registry->get<T>(entity);
        }

        template<typename T>
        bool HasComponent(entt::entity entity)
        {
            return registry->all_of<T>(entity);
        }

        bool IsValid(entt::entity entity) const
        {
            return registry->valid(entity);
        }

        entt::entity GetPrimaryCamera();

        entt::entity GetEntityByUUID(const UUID& uuid) const;
        entt::entity GetEntityByName(const std::string &name);
        const std::string &GetEntityName(entt::entity entity);
        const UUID GetEntityUUID(entt::entity entity);
        void SetPrimaryCamera(entt::entity entity);

        entt::entity GetParentEntity(entt::entity entity) const;
        bool ReparentEntity(entt::entity child, entt::entity newParent);
        bool IsDescendant(entt::entity ancestor, entt::entity entity) const;

        glm::mat4 GetWorldTransform(entt::entity entity) const;
        void SetWorldTransform(entt::entity entity, const glm::mat4& worldTransform);

        entt::registry* registry = nullptr;
        std::unordered_map<UUID, entt::entity> entities;

        glm::vec3 sceneGravity = {0.0f, -9.8f, 0.0f};
        Ref<JoltPhysicsScene> joltPhysicsScene;

    private:
        glm::vec2 m_Viewport = { 1.0f, 1.0f };
        bool m_IsPlaying = false;
    };
}

#endif