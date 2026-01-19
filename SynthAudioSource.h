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
    void setNumHarmonics(int numHarmonics);
    void setNumSubharmonics(int numSubharmonics);

private:
    juce::MidiKeyboardState& keyboardState;
    juce::Synthesiser synth;

    juce::AudioSampleBuffer sineWavetable;
    juce::AudioSampleBuffer sawWavetable;
    juce::AudioSampleBuffer squareWavetable;
    juce::AudioSampleBuffer triangleWavetable;

    int currentWaveform = 0;
    int currentNumHarmonics = 1;
    int currentNumSubharmonics = 0;
    double currentSampleRate = 44100.0;

    void initializeWavetables();
    void regenerateWavetables();
};