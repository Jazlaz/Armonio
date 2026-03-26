#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

class WavetableOscillator
{
public:
    WavetableOscillator() = default;

    // Activate this oscillator with a wavetable reference
    void setWavetable(const juce::AudioSampleBuffer& wavetableToUse)
    {
        tableData = wavetableToUse.getReadPointer(0);
        tableSize = wavetableToUse.getNumSamples() - 1;
        active    = true;
    }

    void deactivate()
    {
        active       = false;
        tableDelta   = 0.0f;
        detunedDelta = 0.0f;
        currentIndex = 0.0f;
        detunedIndex = 0.0f;
    }

    bool isActive() const { return active; }

    void setFrequency(float frequency, float sampleRate)
    {
        tableDelta = frequency * ((float)tableSize / sampleRate);
    }

    // Second phase accumulator for saw detune — avoids a separate oscillator object
    void setDetuneFrequency(float frequency, float sampleRate)
    {
        detunedDelta = frequency * ((float)tableSize / sampleRate);
        // Start detuned phase at a random offset for chorus spread
        detunedIndex = juce::Random::getSystemRandom().nextFloat() * (float)tableSize;
    }

    void setRandomPhase()
    {
        currentIndex = juce::Random::getSystemRandom().nextFloat() * (float)tableSize;
    }

    void setPhase(float phase)
    {
        currentIndex = std::fmod(phase, (float)tableSize);
        if (currentIndex < 0.0f)
            currentIndex += (float)tableSize;
    }

    float getCurrentIndex() const { return currentIndex; }

    forcedinline float getNextSample() noexcept
    {
        auto index0 = (unsigned int)currentIndex;
        auto frac   = currentIndex - (float)index0;

        float sample = tableData[index0] + frac * (tableData[index0 + 1] - tableData[index0]);

        currentIndex += tableDelta;
        if (currentIndex > (float)tableSize)
            currentIndex -= (float)tableSize;

        return sample;
    }

    // Reads main + detuned phase in a single call (one table pointer, no extra cache miss)
    forcedinline float getNextSamplePair(float& detunedSample) noexcept
    {
        auto  idx0   = (unsigned int)currentIndex;
        float frac   = currentIndex - (float)idx0;
        float s1     = tableData[idx0] + frac * (tableData[idx0 + 1] - tableData[idx0]);
        currentIndex += tableDelta;
        if (currentIndex > (float)tableSize) currentIndex -= (float)tableSize;

        auto  didx0  = (unsigned int)detunedIndex;
        float dfrac  = detunedIndex - (float)didx0;
        detunedSample = tableData[didx0] + dfrac * (tableData[didx0 + 1] - tableData[didx0]);
        detunedIndex += detunedDelta;
        if (detunedIndex > (float)tableSize) detunedIndex -= (float)tableSize;

        return s1;
    }

    void stop()  { tableDelta = 0.0f; }
    void reset() { currentIndex = 0.0f; }

private:
    const float* tableData = nullptr;
    int   tableSize    = 0;
    bool  active       = false;
    float currentIndex  = 0.0f;
    float tableDelta    = 0.0f;
    float detunedIndex  = 0.0f;
    float detunedDelta  = 0.0f;
};
