#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "SynthAudioSource.h"

#if JUCE_LINUX
 #include <fcntl.h>
 #include <unistd.h>
 #include <sys/ioctl.h>
 #include <linux/spi/spidev.h>
#endif

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
    float muteGain { 1.0f };   // smoothed 0..1, ramped per-block to avoid clicks

    int getActiveVoiceCount() const noexcept { return synthAudioSource.getActiveVoiceCount(); }

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
    std::atomic<bool> rotaryEnabled { false };
    float             rotaryGain    = 0.0f;   // smoothed 0..1 crossfade between dry and Leslie wet
    std::atomic<bool> rotaryFast    { false };

    static constexpr int kRotaryBufSize = 8192; // power-of-2, ~186ms at 44100
    std::array<float, kRotaryBufSize> hornBuffer{};
    std::array<float, kRotaryBufSize> drumBuffer{};
    int   hornWrite = 0;
    int   drumWrite = 0;

    // Leslie rotation state — tracked as sin/cos pairs so the per-sample trig
    // calls become a rotation-matrix recursion (4 muladds/sample, no std::sin).
    // cDelta/sDelta for the per-sample step are recomputed once per block from
    // hornSpeed/drumSpeed.
    float sHorn = 0.0f, cHorn = 1.0f;   // sin/cos of horn phase
    float sDrum = 0.0f, cDrum = 1.0f;   // sin/cos of drum phase
    float hornSpeed = 0.8f;  // Hz (current, ramps toward target)
    float drumSpeed = 0.6f;

    float hornCenterDelay = 0.0f; // samples
    float hornDelayDepth  = 0.0f;
    float drumCenterDelay = 0.0f;
    float drumDelayDepth  = 0.0f;

    float lpCoeff  = 0.0f;   // one-pole crossover coefficient
    float lpState  = 0.0f;   // mono LP filter state
    double currentSampleRate = 44100.0;

    // ---- Ladder filter (Moog-style 4-pole) ----
    juce::dsp::LadderFilter<float> ladderFilter;
    std::atomic<bool> ladderEnabled { false };

#if JUCE_LINUX
    // ---- SPI / MCP3008 direct ADC reading (Pi only) ----
    int spiFd0 = -1;   // /dev/spidev0.0  chip #1 CE0
    int spiFd1 = -1;   // /dev/spidev0.1  chip #2 CE1
    int readMCP3008 (int fd, int channel) noexcept;
    static constexpr int kAdcThreshold = 8;
    std::array<int, 16> lastAdcRaw {};   // last raw values, threshold detection
#endif

    int timerTickCount = 0;  // counts 50ms ticks; MIDI scan every 40th (= 2s)

    // JI button (CC 32) is now a gate, not a toggle:
    //   tap (press + release with no note in between)  → toggle jiEnabled
    //   hold + Note On                                 → set jiKey to note%12,
    //                                                    enable JI, swallow note
    bool jiButtonHeld          = false;
    bool jiNotePickedDuringHold = false;

    // Knob mode set by CC 90 from the GPIO bridge:
    //   0 = Mode 1 (Hammond + ADSR + harmonics)
    //   1 = Mode 2 (Filter cutoff/res/drive + air high-cut)
    //   2 = Mode 3 (reserved for future use)
    std::atomic<int> knobMode { 0 };
    int lastKnobMode = 0;   // previous mode; when it changes, chip-1 ADC cache is invalidated

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessor)
};