#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

class WavetableOscillator
{
public:
    WavetableOscillator(const juce::AudioSampleBuffer& wavetableToUse)
        : wavetable(wavetableToUse),
          tableSize(wavetable.getNumSamples() - 1)
    {
        jassert(wavetable.getNumChannels() == 1);
    }

    void setFrequency(float frequency, float sampleRate)
    {
        auto tableSizeOverSampleRate = (float)tableSize / sampleRate;
        tableDelta = frequency * tableSizeOverSampleRate;
    }

    void setRandomPhase()
    {
        currentIndex = juce::Random::getSystemRandom().nextFloat() * (float)tableSize;
    }

    forcedinline float getNextSample() noexcept
    {
        // Get integer and fractional parts
        auto index0 = (unsigned int)currentIndex;
        auto index1 = index0 + 1;

        auto frac = currentIndex - (float)index0;

        auto* table = wavetable.getReadPointer(0);
        auto value0 = table[index0];
        auto value1 = table[index1];

        // Linear interpolation
        auto currentSample = value0 + frac * (value1 - value0);

        // Advance and wrap
        currentIndex += tableDelta;
        if (currentIndex > (float)tableSize)
            currentIndex -= (float)tableSize;

        return currentSample;
    }

    void stop()
    {
        tableDelta = 0.0f;
    }

    void reset()
    {
        currentIndex = 0.0f;
    }

private:
    const juce::AudioSampleBuffer& wavetable;
    const int tableSize;
    float currentIndex = 0.0f;
    float tableDelta = 0.0f;
};