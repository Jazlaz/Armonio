#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

class WaveformGenerator
{
public:
    // Creates a single-cycle pure sine wavetable used by all oscillators
    static juce::AudioSampleBuffer createSineWave(unsigned int tableSize,
                                                   float sampleRate = 44100.0f)
    {
        juce::AudioSampleBuffer sineTable;
        sineTable.setSize(1, (int)tableSize + 1);
        sineTable.clear();

        auto* samples   = sineTable.getWritePointer(0);
        auto angleDelta = juce::MathConstants<double>::twoPi / (double)tableSize;

        for (unsigned int i = 0; i < tableSize; ++i)
            samples[i] = (float)std::sin(angleDelta * i);

        samples[tableSize] = samples[0];
        return sineTable;
    }
};
