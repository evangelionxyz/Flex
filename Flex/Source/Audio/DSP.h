// Copyright (c) 2025 Flex Engine | Evangelion Manuhutu

#ifndef FMOD_DSP_H
#define FMOD_DSP_H

#include "AudioEngine.h"
#include <fmod_dsp.h>

namespace flex
{
    struct FmodDsp
    {
        FmodDsp(): m_Dsp(nullptr) {}

        void Release();

        void SetActive(bool active) const;
        bool IsActive() const;

        [[nodiscard]] FMOD::DSP *GetFmodDsp() const { return m_Dsp; }
    protected:
        FMOD::DSP* m_Dsp;
    };

    struct FmodDelay : public FmodDsp
    {
        FmodDelay();
        static Ref<FmodDelay> Create();
    };
        
    struct FmodReverb : FmodDsp
    {
        FmodReverb();

        /// @param millis Units: milliseconds
        /// range[100, 2000], default [1500]
        /// Reverberation decay time at low-frequency
        void SetDecayTime(const float millis) const;
        [[nodiscard]] float GetDecayTime() const;

        /// @param millis Units: milliseconds 
        /// range[0, 300], default [20]
        /// Delay time of first reflection
        void SetEarlyDelay(float millis) const;
        [[nodiscard]] float GetEarlyDelay() const;

        /// @param millis Units: milliseconds
        /// range[0, 300], default [40]
        /// late reverberation dela time relative
        /// to first reflection in milliseconds
        void SetLateDelay(float millis) const;
        [[nodiscard]] float GetLateDelay() const;

        /// @param hertz Units: hertz
        /// range[20, 20000], default [5000]
        /// Reference high frequency for high-frequency decay
        void SetHighFrequencyReference(const float hertz) const;
        [[nodiscard]] float GetHighFrequencyReference() const;

        /// @param percents Units: percents
        /// range[10, 100], default [50]
        /// Reverberation diffusion [echo density]
        void SetDiffusion(const float percents) const;
        [[nodiscard]] float GetDiffusion() const;
        
        /// @param percents Units: percents
        /// range[10, 100], default [50]
        /// Reverberation density [modal density]
        void SetDensity(float percents) const;
        [[nodiscard]] float GetDensity() const;

        /// @param decibels Units: Decibels
        /// range[20, 1000], default [250]
        /// Gain of low-self filter
        void SetLowShelfGain(const float decibels) const;
        [[nodiscard]] float GetLowShelfGain() const;

        /// @param hertz Units: Hertz
        /// range[20, 20000], default [20000]
        /// Cutoff frequency of low-pass filter
        void SetHighCut(const float hertz) const;
        [[nodiscard]] float GetHighCut() const;
        
        /// @param decibels Units: Decibels
        /// range[-80, 20], default [0]
        /// Dry signal level
        void SetDryLevel(const float decibels) const;
        [[nodiscard]] float GetDryLevel() const;
        
        
        /// @param decibels Units: Decibels
        /// range[-80, 20], default [-6]
        /// Reverb signal level
        void SetWetLevel(const float decibels) const;
        [[nodiscard]] float GetWetLevel() const;

        static Ref<FmodReverb> Create();
    };

    struct FmodDistortion : FmodDsp
    {
        FmodDistortion();

        /// @param distortion_level Units: Linear
        /// range [0, 1], default [0.5]
        /// Distortion value
        void SetDistortionLevel(const float distortion_level) const;
        [[nodiscard]] float GetDistortionLevel() const;
        static Ref<FmodDistortion> Create();
    };

    struct FmodChorus : FmodDsp
    {
        FmodChorus();

        /// @param mix Units: percents
        /// range[0, 100], default [50]
        /// Percentage of wet signal in mix
        void SetMix(float mix) const;
        [[nodiscard]] float GetMix() const;
        
        /// @param hertz Units: hertz
        /// range[0, 20], default [0.8]
        ///  Chorus modulation rate
        void SetRate(float hertz) const;
        [[nodiscard]] float GetRate() const;

        /// @param millis Units: milliseconds
        /// range[0, 100], default [3]
        /// Chorus modulation depth
        void SetDepth(float millis) const;
        [[nodiscard]] float GetDepth() const;
        
        static Ref<FmodChorus> Create();
    };

    struct FmodCompressor : FmodDsp
    {
        FmodCompressor();

        /// @param decibels Units: decibels
        /// range[-60, 0], default [0]
        /// Threshold level
        void SetThreshold(float decibels) const;
        [[nodiscard]] float GetThreshold() const;
        
        /// @param ratio Units: linear
        /// range[0, 5.0], default [2.5]
        /// Compression ratio
        void SetRatio(float ratio) const;
        [[nodiscard]] float GetRatio() const;

        /// @param millis Units: milliseconds
        /// range[10, 5000], default [100]
        /// Release time
        void SetRelease(float millis) const;
        [[nodiscard]] float GetRelease() const;


        /// @param decibels Units: decibels
        /// range[-30, 30], default [0]
        /// Make-up gain applied after limiting
        void SetGainMakeup(float decibels) const;
        [[nodiscard]] float GetGainMakeup() const;


        /// @param use Units: boolean
        /// Sidechain signal instead of the input signal
        void SetUseSidechain(bool use) const;

        /// @return bool
        /// false = Independent (compressor per channel
        /// true  = Linked
        /// default = true
        [[nodiscard]] bool IsSidechainLinked() const;
        
        static Ref<FmodCompressor> Create();
    };
}

#endif