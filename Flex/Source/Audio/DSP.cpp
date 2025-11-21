// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#include "DSP.h"

namespace flex
{
    void FmodDsp::Release()
    {
        if (m_Dsp)
        {
            m_Dsp->release();
            m_Dsp = nullptr;
        }
    }

    void FmodDsp::SetActive(const bool active) const
    {
        m_Dsp->setActive(active);    
    }

    bool FmodDsp::IsActive() const
    {
        bool active = false;
        m_Dsp->getActive(&active);
        return active;
    }

    // ====================================
    // Reverb
    // ====================================
    FmodReverb::FmodReverb()
    {
        FMOD_RESULT result = FmodAudio::GetFmodSystem()->createDSPByType(FMOD_DSP_TYPE_SFXREVERB, &m_Dsp);
    }
    void FmodReverb::SetDecayTime(const float millis) const
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_SFXREVERB_DECAYTIME, millis));
    }
    float FmodReverb::GetDecayTime() const
    {
        float v = 0.0f;
        m_Dsp->getParameterFloat(FMOD_DSP_SFXREVERB_DECAYTIME, &v, nullptr, 0);
        return v;
    }

    void FmodReverb::SetEarlyDelay(float millis) const
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_SFXREVERB_EARLYDELAY, millis));
    }

    float FmodReverb::GetEarlyDelay() const
    {
        float v = 0.0f;
        m_Dsp->getParameterFloat(FMOD_DSP_SFXREVERB_EARLYDELAY, &v, nullptr, 0);
        return v;
    }

    void FmodReverb::SetLateDelay(float millis) const
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_SFXREVERB_LATEDELAY, millis));
    }

    float FmodReverb::GetLateDelay() const
    {
        float v = 0.0f;
        m_Dsp->getParameterFloat(FMOD_DSP_SFXREVERB_LATEDELAY, &v, nullptr, 0);
        return v;
    }

    void FmodReverb::SetHighFrequencyReference(const float hertz) const
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_SFXREVERB_HFREFERENCE, hertz));
    }
    float FmodReverb::GetHighFrequencyReference() const
    {
        float v = 0.0f;
        m_Dsp->getParameterFloat(FMOD_DSP_SFXREVERB_HFREFERENCE, &v, nullptr, 0);
        return v;
    }

    void FmodReverb::SetDiffusion(const float percents) const
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_SFXREVERB_DIFFUSION, percents));
    }

    float FmodReverb::GetDiffusion() const
    {
        float v = 0.0f;
        m_Dsp->getParameterFloat(FMOD_DSP_SFXREVERB_DIFFUSION, &v, nullptr, 0);
        return v;
    }

    void FmodReverb::SetDensity(float percents) const
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_SFXREVERB_DENSITY, percents));
    }

    float FmodReverb::GetDensity() const
    {
        float v = 0.0f;
        m_Dsp->getParameterFloat(FMOD_DSP_SFXREVERB_DENSITY, &v, nullptr, 0);
        return v;
    }

    void FmodReverb::SetLowShelfGain(const float decibels) const
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_SFXREVERB_LOWSHELFFREQUENCY, decibels));
    }

    float FmodReverb::GetLowShelfGain() const
    {
        float v = 0.0f;
        m_Dsp->getParameterFloat(FMOD_DSP_SFXREVERB_LOWSHELFFREQUENCY, &v, nullptr, 0);
        return v;
    }

    void FmodReverb::SetHighCut(const float hertz) const
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_SFXREVERB_HIGHCUT, hertz));
    }

    float FmodReverb::GetHighCut() const
    {
        float v = 0.0f;
        m_Dsp->getParameterFloat(FMOD_DSP_SFXREVERB_DRYLEVEL, &v, nullptr, 0);
        return v;
    }

    void FmodReverb::SetDryLevel(const float decibels) const
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_SFXREVERB_DRYLEVEL, decibels));
    }

    float FmodReverb::GetDryLevel() const
    {
        float v = 0.0f;
        m_Dsp->getParameterFloat(FMOD_DSP_SFXREVERB_EARLYLATEMIX, &v, nullptr, 0);
        return v;
    }

    void FmodReverb::SetWetLevel(const float decibels) const
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_SFXREVERB_WETLEVEL, decibels));
    }

    float FmodReverb::GetWetLevel() const
    {
        float v = 0.0f;
        m_Dsp->getParameterFloat(FMOD_DSP_SFXREVERB_WETLEVEL, &v, nullptr, 0);
        return v;
    }

    Ref<FmodReverb> FmodReverb::Create()
    {
        return CreateRef<FmodReverb>();
    }


    // ====================================
    // Distortion
    // ====================================
    FmodDistortion::FmodDistortion()
    {
        FMOD_RESULT result = FmodAudio::GetFmodSystem()->createDSPByType(FMOD_DSP_TYPE_DISTORTION, &m_Dsp);
    }

    void FmodDistortion::SetDistortionLevel(const float value) const
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_DISTORTION_LEVEL, value));
    }

    float FmodDistortion::GetDistortionLevel() const
    {
        float v = 0.0f;
        m_Dsp->getParameterFloat(FMOD_DSP_DISTORTION_LEVEL, &v, nullptr, 0);
        return v;
    }

    Ref<FmodDistortion> FmodDistortion::Create()
    {
        return CreateRef<FmodDistortion>();
    }

    // ====================================
    // Chorus
    // ====================================
    FmodChorus::FmodChorus()
    {
        FMOD_RESULT result = FmodAudio::GetFmodSystem()->createDSPByType(FMOD_DSP_TYPE_CHORUS, &m_Dsp);
    }

    void FmodChorus::SetMix(const float mix) const
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_CHORUS_MIX, mix));
    }

    float FmodChorus::GetMix() const
    {
        float v = 0.0f;
        (m_Dsp->getParameterFloat(FMOD_DSP_CHORUS_MIX, &v, nullptr, 0));
        return v;
    }

    void FmodChorus::SetRate(const float hertz) const
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_CHORUS_RATE, hertz));
    }

    float FmodChorus::GetRate() const
    {
        float v = 0.0f;
        (m_Dsp->getParameterFloat(FMOD_DSP_CHORUS_RATE, &v, nullptr, 0));
        return v;
    }

    void FmodChorus::SetDepth(const float millis) const
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_CHORUS_DEPTH, millis));
    }

    float FmodChorus::GetDepth() const
    {
        float v = 0.0f;
        (m_Dsp->getParameterFloat(FMOD_DSP_CHORUS_DEPTH, &v, nullptr, 0));
        return v;
    }

    Ref<FmodChorus> FmodChorus::Create()
    {
        return CreateRef<FmodChorus>();
    }

    // ====================================
    // Compressor
    // ====================================
    FmodCompressor::FmodCompressor()
    {
        FMOD_RESULT result = FmodAudio::GetFmodSystem()->createDSPByType(FMOD_DSP_TYPE_COMPRESSOR, &m_Dsp);
    }

    void FmodCompressor::SetThreshold(float decibels) const 
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_COMPRESSOR_THRESHOLD, decibels));
    }

    float FmodCompressor::GetThreshold() const 
    {
        float v = 0.0f;
        (m_Dsp->getParameterFloat(FMOD_DSP_COMPRESSOR_THRESHOLD, &v, nullptr, 0));
        return v;
    }

    void FmodCompressor::SetRatio(float ratio) const 
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_COMPRESSOR_RATIO, ratio));
    }

    float FmodCompressor::GetRatio() const 
    {
        float v = 0.0f;
        (m_Dsp->getParameterFloat(FMOD_DSP_COMPRESSOR_RATIO, &v, nullptr, 0));
        return v;
    }

    void FmodCompressor::SetRelease(float millis) const 
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_COMPRESSOR_RELEASE, millis));
    }

    float FmodCompressor::GetRelease() const 
    {
        float v = 0.0f;
        (m_Dsp->getParameterFloat(FMOD_DSP_COMPRESSOR_RELEASE, &v, nullptr, 0));
        return v;
    }

    void FmodCompressor::SetGainMakeup(float decibels) const 
    {
        (m_Dsp->setParameterFloat(FMOD_DSP_COMPRESSOR_GAINMAKEUP, decibels));
    }

    float FmodCompressor::GetGainMakeup() const 
    {
        float v = 0.0f;
        (m_Dsp->getParameterFloat(FMOD_DSP_COMPRESSOR_GAINMAKEUP, &v, nullptr, 0));
        return v;
    }

    void FmodCompressor::SetUseSidechain(bool use) const 
    {
        // FMOD_RESULT result = m_Dsp->setParameterBool(FMOD_DSP_COMPRESSOR_USESIDECHAIN, use);
    }

    bool FmodCompressor::IsSidechainLinked() const 
    {
        bool v = false;
        (m_Dsp->getParameterBool(FMOD_DSP_COMPRESSOR_LINKED, &v, nullptr, 0));
        return v;
    }

    Ref<FmodCompressor> FmodCompressor::Create() 
    {
        return CreateRef<FmodCompressor>();
    }

    // ====================================
    // Delay
    // ====================================
    FmodDelay::FmodDelay()
    {
    }

    Ref<FmodDelay> FmodDelay::Create()
    {
        return CreateRef<FmodDelay>();
    }
}
