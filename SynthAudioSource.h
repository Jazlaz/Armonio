#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "WavetableSound.h"
#include "WaveformGenerator.h"

class SynthAudioSource : public juce::AudioSource
{
public:
    explicit SynthAudioSource(juce::MidiKeyboardState& keyState);

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill,
                           juce::MidiBuffer& midiMessages);

    void setWaveform(int waveformType);
    void setADSRParameters(float attack, float decay, float sustain, float release);
    void setNumHarmonics(float numHarmonics);
    void setNumSubharmonics(float numSubharmonics);
    void setLFODepth(float depth);
    void setLFOSpeed(float speed);
    void setLFOEnabled(bool enabled);
    void setLFOMode(int mode);
    void setHammondFifth(float level);
    void setHammondSubOctave(float level);
    void setHammondOctaveUp(float level);
    void setHammondThirdHarmonic(float level);
    void setHammondTwoOctavesUp(float level);
    void setHammondFifthHarmonic(float level);
    void setHammondSixthHarmonic(float level);
    void setHammondThreeOctavesUp(float level);
    void setChime2Enabled(bool enabled);
    void setChime3Enabled(bool enabled);

private:
    juce::MidiKeyboardState& keyboardState;
    juce::Synthesiser synth;

    juce::AudioSampleBuffer sineWavetable;  // single pure sine, shared by all oscillators

    int    currentWaveform        = 0;
    float  currentNumHarmonics    = 1.0f;
    float  currentNumSubharmonics = 0.0f;
    double currentSampleRate      = 44100.0;
    float  currentLFODepth        = 0.0f;
    float  currentLFOSpeed        = 0.5f;
    bool   currentLFOEnabled      = false;
    int    currentLFOMode         = 0;
    float  currentHammondFifth           = 0.0f;
    float  currentHammondSubOctave       = 0.0f;
    float  currentHammondOctaveUp        = 0.0f;
    float  currentHammondThirdHarmonic   = 0.0f;
    float  currentHammondTwoOctavesUp    = 0.0f;
    float  currentHammondFifthHarmonic   = 0.0f;
    float  currentHammondSixthHarmonic   = 0.0f;
    float  currentHammondThreeOctavesUp  = 0.0f;
    bool   currentChime2Enabled          = false;
    bool   currentChime3Enabled          = false;

    void initializeWavetables();
    void pushStateToVoices();
};
