// Copyright (c) 2025 Evangelion Manuhutu

#ifndef SCRIPTABLE_ENTITY_H
#define SCRIPTABLE_ENTITY_H

#include "Scene.h"

namespace flex
{
    class Scene;
    class ScriptableEntity
    {
    public:
        ScriptableEntity(Scene *scene, entt::entity entity)
            : m_Scene(scene), m_Entity(entity)
        {
        }

        virtual ~ScriptableEntity() {};

        template<typename T>
        T GetComponent()
        {
            return m_Scene->GetComponent<T>(m_Entity);
        }

    protected:
        virtual void OnStart() {};
        virtual void OnStop() {};
        virtual void OnUpdate(float deltaTime) {};

    private:
        Scene *m_Scene = nullptr;
        entt::entity m_Entity;

        friend class Scene;
        friend struct NativeScriptComponent;
    };
}

#endif