// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "AudioEngine.h"
#include "Sound.h"

#include <ranges>

namespace flex
{
    static FMOD_RESULT result;
    static FmodAudio *s_FMOD_Instance = nullptr;

    void FmodAudio::SetMasterVolume(const float volume)
    {
        result = s_FMOD_Instance->m_MasterGroup->setVolume(volume);
    }

    void FmodAudio::MuteMaster(const bool mute)
    {
        result = s_FMOD_Instance->m_MasterGroup->setMute(mute);
    }

    void FmodAudio::Update(const float delta_time)
    {
        s_FMOD_Instance->m_System->update();
        for (const auto& val : s_FMOD_Instance->m_SoundMap | std::views::values)
        {
            val->Update(delta_time);
        }
    }

    void FmodAudio::Init()
    {
        s_FMOD_Instance = new FmodAudio();

        result = FMOD::System_Create(&s_FMOD_Instance->m_System);
        result = s_FMOD_Instance->m_System->init(32, FMOD_INIT_NORMAL, nullptr);
        s_FMOD_Instance->m_MasterGroup = FmodAudio::CreateChannelGroup("Master");

        // Initialize listener position
        s_FMOD_Instance->listenerPos = { 0.0f,0.0f,0.0f };
        s_FMOD_Instance->listenerVel = { 0.0f,0.0f,0.0f };
        s_FMOD_Instance->listenerForward = { 0.0f,0.0f,1.0f };
        s_FMOD_Instance->listenerUp = { 0.0f,1.0f,0.0f };
    }

    void FmodAudio::Shutdown()
    {
        for (auto &s : s_FMOD_Instance->m_SoundMap | std::views::values)
        {
            s->Release();
        }

        s_FMOD_Instance->m_SoundMap.clear();

        result = s_FMOD_Instance->m_System->close();
        result = s_FMOD_Instance->m_System->release();

        delete s_FMOD_Instance;
    }

    FMOD::ChannelGroup* FmodAudio::CreateChannelGroup(const std::string &name)
    {
        FMOD::ChannelGroup* group = nullptr;
        s_FMOD_Instance->m_System->createChannelGroup(name.c_str(), &group);
        group->setMode(FMOD_LOOP_NORMAL);
        s_FMOD_Instance->m_ChannelGroups[name] = group;
        return group;
    }
    
    std::unordered_map<std::string, FMOD::ChannelGroup*> FmodAudio::GetChannelGroupMap()
    {
        return s_FMOD_Instance->m_ChannelGroups;    
    }

    FMOD::ChannelGroup* FmodAudio::GetChannelGroup(const std::string& name)
    {
        if (s_FMOD_Instance->m_ChannelGroups.contains(name))
            return s_FMOD_Instance->m_ChannelGroups[name];
        return nullptr;
    }

    FmodAudio &FmodAudio::GetInstance()
    {
        return *s_FMOD_Instance;
    }

    FMOD::System* FmodAudio::GetFmodSystem()
    {
        return s_FMOD_Instance->m_System;
    }

    FMOD::ChannelGroup* FmodAudio::GetMasterChannel()
    {
        return s_FMOD_Instance->m_MasterGroup;
    }

    float FmodAudio::GetMasterVolume()
    {
        float volume = 0.0f;
        s_FMOD_Instance->m_MasterGroup->getVolume(&volume);
        return volume;
    }

    void FmodAudio::InsertFmodSound(const std::string &name, const Ref<FmodSound>& sound)
    {
        s_FMOD_Instance->m_SoundMap[name] = sound;
    }

    void FmodAudio::RemoveFmodSound(const std::string &name)
    {
        if (const auto it = s_FMOD_Instance->m_SoundMap.find(name); it != s_FMOD_Instance->m_SoundMap.end())
        {
            s_FMOD_Instance->m_SoundMap.erase(it);
        }
    }
}
