#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SynthAudioSource.h"

//==============================================================================
class AudioPluginAudioProcessor final : public juce::AudioProcessor,
                                       public juce::AudioProcessorValueTreeState::Listener,
                                       public juce::Timer,
                                       private juce::MidiInputCallback
{
public:
    //==============================================================================
    AudioPluginAudioProcessor();
    ~AudioPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // APVTS Listener - called when parameters change
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Accessor for APVTS (used by Editor to attach sliders)
    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }

    juce::MidiKeyboardState keyboardState;
    std::atomic<bool> muted { false };

private:
    // ---- MIDI device auto-detection ----
    void timerCallback() override;
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& msg) override;

    juce::MidiMessageCollector              midiDeviceCollector;
    std::vector<std::unique_ptr<juce::MidiInput>> ownedMidiInputs;

    SynthAudioSource synthAudioSource;

    // The parameter state manager
    juce::AudioProcessorValueTreeState apvts;

    // Helper to create all parameters
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Helper to update synth when parameters change
    void updateSynthParameters();

    // ---- Rotary (Leslie) effect ----
    bool  rotaryEnabled = false;
    bool  rotaryFast    = false;

    static constexpr int kRotaryBufSize = 8192; // power-of-2, ~186ms at 44100
    std::array<float, kRotaryBufSize> hornBuffer{};
    std::array<float, kRotaryBufSize> drumBuffer{};
    int   hornWrite = 0;
    int   drumWrite = 0;

    float hornAngle = 0.0f;  // 0..1 normalised phase
    float drumAngle = 0.0f;
    float hornSpeed = 0.8f;  // Hz (current, ramps toward target)
    float drumSpeed = 0.6f;

    float hornCenterDelay = 0.0f; // samples
    float hornDelayDepth  = 0.0f;
    float drumCenterDelay = 0.0f;
    float drumDelayDepth  = 0.0f;

    float lpCoeff  = 0.0f;   // one-pole crossover coefficient
    float lpState  = 0.0f;   // mono LP filter state
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};