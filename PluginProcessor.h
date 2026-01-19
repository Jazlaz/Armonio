#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SynthAudioSource.h"

//==============================================================================
class AudioPluginAudioProcessor final : public juce::AudioProcessor,
                                       public juce::AudioProcessorValueTreeState::Listener
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

private:
    SynthAudioSource synthAudioSource;

    // The parameter state manager
    juce::AudioProcessorValueTreeState apvts;

    // Helper to create all parameters
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Helper to update synth when parameters change
    void updateSynthParameters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};