// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "Sound.h"
#include "AudioEngine.h"

#include "SDL3/SDL_log.h"

#include <algorithm>

namespace flex
{
    FmodSound::FmodSound(std::string name) : m_Sound(nullptr), m_Channel(nullptr),
                                            m_Name(std::move(name)), m_FadeInStartMs(0), m_FadeInEndMs(0),
                                            m_FadeOutStartMs(0), m_FadeOutEndMs(0), m_ChannelGroup(nullptr)
    {}

    void FmodSound::Play()
    {
        const FMOD_RESULT result = FmodAudio::GetFmodSystem()->playSound(
            m_Sound,
            m_ChannelGroup ? m_ChannelGroup : FmodAudio::GetMasterChannel(),
            false, // start paused
            &m_Channel
        );

        // remove dsp
        int dsp_count = 0;
        m_Channel->getNumDSPs((&dsp_count));
        for (int i = 0; i < dsp_count; i++)
        {
            FMOD::DSP* dsp;
            m_Channel->getDSP(i, &dsp);
            m_Channel->removeDSP(dsp);
        }
        
        // re-add dsp
        for (const auto &dsp : m_DSPs)
        {
            m_Channel->addDSP(0, dsp);
        }
    }

    void FmodSound::Stop() const
    {
        m_Channel->stop();
    }

    void FmodSound::Pause() const
    {
        m_Channel->setPaused(true);
    }

    void FmodSound::Resume() const
    {
        m_Channel->setPaused(false);
    }

    void FmodSound::SetName(const std::string &name)
    {
        m_Name = name;
    }

    void FmodSound::SetPan(const float pan) const
    {
        m_Channel->setPan(pan);
    }

    void FmodSound::SetVolume(float volume) const
    {
        volume = std::clamp(volume, 0.0f, 1.0f);
        m_Channel->setVolume(volume);
    }

    void FmodSound::SetPitch(const float pitch) const
    {
        m_Channel->setPitch(pitch);
    }

    void FmodSound::SetMode(const FMOD_MODE mode) const
    {
        m_Channel->setMode(mode);    
    }

    void FmodSound::SetFadeIn(const uint32_t fade_in_start_ms, const uint32_t fade_in_end_ms)
    {
        m_FadeInStartMs = fade_in_start_ms;
        m_FadeInEndMs = fade_in_end_ms;
    }

    void FmodSound::SetFadeOut(const uint32_t fade_out_start_ms, const uint32_t fade_out_end_ms)
    {
        m_FadeOutStartMs = fade_out_start_ms;
        m_FadeOutEndMs = fade_out_end_ms;
    }

    void FmodSound::AddToChannelGroup(FMOD::ChannelGroup* channel_group)
    {
        m_ChannelGroup = channel_group;
    }

    void FmodSound::Release()
    {
        if (m_Sound)
        {
            m_Sound->release();
            m_Sound = nullptr;
        }

        for (auto &dsp : m_DSPs)
        {
            dsp->release();
        }
    }

    float FmodSound::GetPitch() const
    {
        float pitch;
        m_Channel->getPitch(&pitch);
        return pitch;
    }

    float FmodSound::GetVolume() const
    {
        float volume;
        m_Channel->getVolume(&volume);
        return volume;
    }

    void FmodSound::Update([[maybe_unused]] float delta_time) const
    {
        UpdateFading();
    }

    void FmodSound::AddDsp(FMOD::DSP* dsp)
    {
        m_DSPs.push_back(dsp);
    }

    FMOD::Sound* FmodSound::GetFmodSound() const
    {
        return m_Sound;
    }

    FMOD::Channel* FmodSound::GetFmodChannel() const
    {
        return m_Channel;
    }

    const std::string& FmodSound::GetName() const
    {
        return m_Name;
    }

    bool FmodSound::IsPlaying() const
    {
        bool is_playing = false;
        m_Channel->isPlaying(&is_playing);
        return is_playing;
    }

    bool FmodSound::IsPaused() const
    {
        bool paused = false;
        m_Channel->getPaused(&paused);
        return paused;
    }

    uint32_t FmodSound::GetLengthMs() const
    {
        uint32_t length;
        m_Sound->getLength(&length, FMOD_TIMEUNIT_MS);
        return length;
    }

    uint32_t FmodSound::GetPositionMs() const
    {
        uint32_t position;
        m_Channel->getPosition(&position, FMOD_TIMEUNIT_MS);
        return position;
    }

    FMOD::ChannelGroup* FmodSound::GetChannelGroup() const
    {
        return m_ChannelGroup;
    }

    Ref<FmodSound> FmodSound::Create(const std::string &name, const std::string &filepath, const FMOD_MODE mode)
    {
        Ref<FmodSound> fmod_sound = CreateRef<FmodSound>(name);
        FmodAudio::GetFmodSystem()->createSound(filepath.c_str(), mode, nullptr, &fmod_sound->m_Sound);
        
        const FMOD_RESULT result = FmodAudio::GetFmodSystem()->playSound(
                fmod_sound->m_Sound,
                fmod_sound->m_ChannelGroup ? fmod_sound->m_ChannelGroup : FmodAudio::GetMasterChannel(),
                true, // start paused
                &fmod_sound->m_Channel
            );
        
        SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "[FMOD Sound] Load sound '%s'", filepath.c_str());
        
        FmodAudio::InsertFmodSound(name, fmod_sound);
        return fmod_sound;
    }

    Ref<FmodSound> FmodSound::CreateStream(const std::string& name, const std::string& filepath, const FMOD_MODE mode)
    {
        Ref<FmodSound> fmod_sound = CreateRef<FmodSound>(name);
        FmodAudio::GetFmodSystem()->createStream(filepath.c_str(), mode, nullptr, &fmod_sound->m_Sound);
        
        const FMOD_RESULT result = FmodAudio::GetFmodSystem()->playSound(
            fmod_sound->m_Sound,
            fmod_sound->m_ChannelGroup ? fmod_sound->m_ChannelGroup : FmodAudio::GetMasterChannel(),
            true, // start paused
            &fmod_sound->m_Channel
        );
                
        FmodAudio::InsertFmodSound(name, fmod_sound);
        return fmod_sound;
    }

    void FmodSound::UpdateFading() const
    {
        uint32_t position = 0;
        
        m_Channel->getPosition(&position, FMOD_TIMEUNIT_MS);
        float volume = GetVolume();
        if (position >= m_FadeInStartMs && position <= m_FadeInEndMs)
        {
            const float fade_progress = static_cast<float>(position - m_FadeInStartMs)
                / static_cast<float>(m_FadeInEndMs - m_FadeInStartMs);
            volume = fade_progress;
        }

        if (position >= m_FadeOutStartMs && position <= m_FadeOutEndMs)
        {
            const float fade_progress = static_cast<float>(position - m_FadeOutStartMs)
                / static_cast<float>(m_FadeOutEndMs - m_FadeOutStartMs);
            volume = 1.0f - fade_progress;
        }

        SetVolume(volume);
    }
}
