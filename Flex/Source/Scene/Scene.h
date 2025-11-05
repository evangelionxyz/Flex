// Copyright (c) 2025 Evangelion Manuhutu

#ifndef SCENE_H
#define SCENE_H

#include "entt.h"
#include "Core/UUID.h"

#include "Physics/JoltPhysics.h"

#include <unordered_map>

namespace flex
{
    class Scene
    {
    public:
        Scene();
        ~Scene();

        entt::entity CreateEntity(const std::string& name, const UUID &uuid = UUID());

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

        void DestroyEntity(const entt::entity entity);
        entt::entity GetEntity(const UUID& uuid);

        entt::registry* registry = nullptr;
        std::unordered_map<UUID, entt::entity> entities;
    };
}

#endif