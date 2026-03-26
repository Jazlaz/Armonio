#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioPluginAudioProcessor::AudioPluginAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      synthAudioSource(keyboardState),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // Listen for parameter changes
    apvts.addParameterListener("waveform", this);
    apvts.addParameterListener("harmonics", this);
    apvts.addParameterListener("subharmonics", this);
    apvts.addParameterListener("attack", this);
    apvts.addParameterListener("decay", this);
    apvts.addParameterListener("sustain", this);
    apvts.addParameterListener("release", this);
    apvts.addParameterListener("lfoDepth",   this);
    apvts.addParameterListener("lfoSpeed",   this);
    apvts.addParameterListener("lfoEnabled",   this);
    apvts.addParameterListener("lfoMode",      this);
    apvts.addParameterListener("hammondFifth",     this);
    apvts.addParameterListener("hammondSubOctave", this);
    apvts.addParameterListener("hammondChime2",        this);
    apvts.addParameterListener("hammondChime3",        this);
    apvts.addParameterListener("hammondOctaveUp",       this);
    apvts.addParameterListener("hammondThirdHarmonic",  this);
    apvts.addParameterListener("hammondTwoOctavesUp",   this);
    apvts.addParameterListener("hammondFifthHarmonic",  this);
    apvts.addParameterListener("hammondSixthHarmonic",  this);
    apvts.addParameterListener("hammondThreeOctavesUp", this);
    apvts.addParameterListener("rotaryEnabled", this);
    apvts.addParameterListener("rotaryFast",    this);

    // Start scanning for MIDI devices immediately, then every 2 seconds
    startTimer(2000);
    timerCallback();
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    // Clean up listeners
    apvts.removeParameterListener("waveform", this);
    apvts.removeParameterListener("harmonics", this);
    apvts.removeParameterListener("subharmonics", this);
    apvts.removeParameterListener("attack", this);
    apvts.removeParameterListener("decay", this);
    apvts.removeParameterListener("sustain", this);
    apvts.removeParameterListener("release", this);
    apvts.removeParameterListener("lfoDepth",   this);
    apvts.removeParameterListener("lfoSpeed",   this);
    apvts.removeParameterListener("lfoEnabled",   this);
    apvts.removeParameterListener("lfoMode",      this);
    apvts.removeParameterListener("hammondFifth",     this);
    apvts.removeParameterListener("hammondSubOctave", this);
    apvts.removeParameterListener("hammondChime2",         this);
    apvts.removeParameterListener("hammondChime3",         this);
    apvts.removeParameterListener("hammondOctaveUp",       this);
    apvts.removeParameterListener("hammondThirdHarmonic",  this);
    apvts.removeParameterListener("hammondTwoOctavesUp",   this);
    apvts.removeParameterListener("hammondFifthHarmonic",  this);
    apvts.removeParameterListener("hammondSixthHarmonic",  this);
    apvts.removeParameterListener("hammondThreeOctavesUp", this);
    apvts.removeParameterListener("rotaryEnabled", this);
    apvts.removeParameterListener("rotaryFast",    this);

    stopTimer();
    ownedMidiInputs.clear();
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AudioPluginAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Waveform selector (0=Sine, 1=Square, 2=Triangle)
    layout.add(std::make_unique<juce::AudioParameterInt>(
        "waveform",
        "Waveform",
        0, 2, 0));

    // Harmonics
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "harmonics",
        "Harmonics",
        juce::NormalisableRange<float>(1.0f, 16.0f, 0.01f),
        1.0f));

    // Subharmonics
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "subharmonics",
        "Subharmonics",
        juce::NormalisableRange<float>(0.0f, 8.0f, 0.01f),
        0.0f));

    // Attack time (1ms to 1 second)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "attack",
        "Attack",
        juce::NormalisableRange<float>(0.001f, 1.0f, 0.001f),
        0.01f));

    // Decay time (1ms to 2 seconds)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "decay",
        "Decay",
        juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f),
        0.1f));

    // Sustain level (0 to 1)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "sustain",
        "Sustain",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
        0.8f));

    // Release time (1ms to 5 seconds)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "release",
        "Release",
        juce::NormalisableRange<float>(0.001f, 5.0f, 0.001f),
        0.3f));

    // Harmonic LFO depth (skewed for small-value precision)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "lfoDepth",
        "LFO Depth",
        juce::NormalisableRange<float>(0.0f, 0.05f, 0.0001f, 0.5f),
        0.0f));

    // Harmonic LFO speed (full range covers both SLOW and FAST sub-ranges)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "lfoSpeed",
        "LFO Speed",
        juce::NormalisableRange<float>(0.001f, 0.5f, 0.0001f, 0.5f),
        0.05f));

    // LFO on/off
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "lfoEnabled", "LFO Enabled", false));

    // LFO mode: 0=all, 1=every 2nd, 2=every 3rd, 3=every 4th, 4=fundamental only
    layout.add(std::make_unique<juce::AudioParameterInt>(
        "lfoMode", "LFO Mode", 0, 4, 0));

    // Hammond fifth (quint) mix level
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "hammondFifth",
        "Hammond Fifth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.0f));

    // Hammond sub-octave mix level
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "hammondSubOctave",
        "Hammond Sub Octave",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
        0.0f));

    // Hammond chime buttons
    layout.add(std::make_unique<juce::AudioParameterBool>("hammondChime2", "Chime 2nd", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("hammondChime3", "Chime 3rd", false));

    // Hammond register levels
    layout.add(std::make_unique<juce::AudioParameterFloat>("hammondOctaveUp",       "Octave Up",        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("hammondThirdHarmonic",  "Oct + Fifth",      juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("hammondTwoOctavesUp",   "Two Octaves Up",   juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("hammondFifthHarmonic",  "2 Oct + Third",    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("hammondSixthHarmonic",  "2 Oct + Fifth",    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("hammondThreeOctavesUp", "Three Octaves Up", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));

    // Rotary (Leslie) effect
    layout.add(std::make_unique<juce::AudioParameterBool>("rotaryEnabled", "Rotary Enabled", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("rotaryFast",    "Rotary Fast",    false));

    return layout;
}

//==============================================================================
void AudioPluginAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "waveform")
    {
        synthAudioSource.setWaveform((int)newValue);
    }
    else if (parameterID == "harmonics")
    {
        synthAudioSource.setNumHarmonics(newValue);
    }
    else if (parameterID == "subharmonics")
    {
        synthAudioSource.setNumSubharmonics(newValue);
    }
    else if (parameterID == "attack" ||
             parameterID == "decay" ||
             parameterID == "sustain" ||
             parameterID == "release")
    {
        updateSynthParameters();
    }
    else if (parameterID == "lfoDepth")   { synthAudioSource.setLFODepth(newValue); }
    else if (parameterID == "lfoSpeed")   { synthAudioSource.setLFOSpeed(newValue); }
    else if (parameterID == "lfoEnabled") { synthAudioSource.setLFOEnabled(newValue > 0.5f); }
    else if (parameterID == "lfoMode")      { synthAudioSource.setLFOMode((int)newValue); }
    else if (parameterID == "hammondFifth")     { synthAudioSource.setHammondFifth(newValue); }
    else if (parameterID == "hammondSubOctave") { synthAudioSource.setHammondSubOctave(newValue); }
    else if (parameterID == "hammondChime2")         { synthAudioSource.setChime2Enabled(newValue > 0.5f); }
    else if (parameterID == "hammondChime3")         { synthAudioSource.setChime3Enabled(newValue > 0.5f); }
    else if (parameterID == "hammondOctaveUp")       { synthAudioSource.setHammondOctaveUp(newValue); }
    else if (parameterID == "hammondThirdHarmonic")  { synthAudioSource.setHammondThirdHarmonic(newValue); }
    else if (parameterID == "hammondTwoOctavesUp")   { synthAudioSource.setHammondTwoOctavesUp(newValue); }
    else if (parameterID == "hammondFifthHarmonic")  { synthAudioSource.setHammondFifthHarmonic(newValue); }
    else if (parameterID == "hammondSixthHarmonic")  { synthAudioSource.setHammondSixthHarmonic(newValue); }
    else if (parameterID == "hammondThreeOctavesUp") { synthAudioSource.setHammondThreeOctavesUp(newValue); }
    else if (parameterID == "rotaryEnabled") { rotaryEnabled = (newValue > 0.5f); }
    else if (parameterID == "rotaryFast")    { rotaryFast    = (newValue > 0.5f); }
}

void AudioPluginAudioProcessor::updateSynthParameters()
{
    float attack = apvts.getRawParameterValue("attack")->load();
    float decay = apvts.getRawParameterValue("decay")->load();
    float sustain = apvts.getRawParameterValue("sustain")->load();
    float release = apvts.getRawParameterValue("release")->load();

    synthAudioSource.setADSRParameters(attack, decay, sustain, release);
}

//==============================================================================
const juce::String AudioPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioPluginAudioProcessor::getNumPrograms()
{
    return 1;
}

int AudioPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioPluginAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String AudioPluginAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void AudioPluginAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void AudioPluginAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    synthAudioSource.prepareToPlay(samplesPerBlock, sampleRate);
    updateSynthParameters();
    midiDeviceCollector.reset(sampleRate);

    currentSampleRate = sampleRate;

    // Rotary delay times (in samples)
    hornCenterDelay = 0.005f * (float)sampleRate;  // 5 ms
    hornDelayDepth  = 0.0002f * (float)sampleRate;  // 2 ms
    drumCenterDelay = 0.007f * (float)sampleRate;  // 7 ms
    drumDelayDepth  = 0.0002f * (float)sampleRate;  // 2 ms

    // Crossover LP coefficient (~800 Hz)
    lpCoeff = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 800.0f / (float)sampleRate);

    hornBuffer.fill(0.0f);
    drumBuffer.fill(0.0f);
    hornWrite = drumWrite = 0;
    lpState   = 0.0f;
    hornAngle = drumAngle = 0.0f;
    hornSpeed = rotaryFast ? 6.7f : 0.8f;
    drumSpeed = rotaryFast ? 5.7f : 0.6f;
}

void AudioPluginAudioProcessor::releaseResources()
{
    synthAudioSource.releaseResources();
}

bool AudioPluginAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void AudioPluginAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Merge directly-opened MIDI devices into the buffer
    midiDeviceCollector.removeNextBlockOfMessages(midiMessages, buffer.getNumSamples());

    // Minilab MK2 MIDI CC mapping
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (!msg.isController()) continue;

        const int cc  = msg.getControllerNumber();
        const int val = msg.getControllerValue();

        // Buttons: waveform select (fire on press only)
        if (val > 0)
        {
            if      (cc == 20) apvts.getParameter("waveform")->setValueNotifyingHost(0.0f);
            else if (cc == 21) apvts.getParameter("waveform")->setValueNotifyingHost(0.5f);
            else if (cc == 22) apvts.getParameter("waveform")->setValueNotifyingHost(1.0f);
        }

        // Buttons: chime toggles (on = 127, off = 0)
        if (cc == 23) apvts.getParameter("hammondChime2")->setValueNotifyingHost(val > 0 ? 1.0f : 0.0f);
        if (cc == 24) apvts.getParameter("hammondChime3")->setValueNotifyingHost(val > 0 ? 1.0f : 0.0f);

        // Buttons: rotary + mute toggles
        if (cc == 25) apvts.getParameter("rotaryEnabled")->setValueNotifyingHost(val > 0 ? 1.0f : 0.0f);
        if (cc == 26) apvts.getParameter("rotaryFast")->setValueNotifyingHost(val > 0 ? 1.0f : 0.0f);
        if (cc == 27) muted.store(val > 0);

        // Knobs: normalized 0-127 -> 0.0-1.0
        const float norm = val / 127.0f;
        // Hammond sliders (in tab order)
        if (cc == 73) apvts.getParameter("hammondOctaveUp")->setValueNotifyingHost(norm);
        if (cc == 74) apvts.getParameter("hammondThirdHarmonic")->setValueNotifyingHost(norm);
        if (cc == 75) apvts.getParameter("hammondTwoOctavesUp")->setValueNotifyingHost(norm);
        if (cc == 76) apvts.getParameter("hammondFifthHarmonic")->setValueNotifyingHost(norm);
        if (cc == 77) apvts.getParameter("hammondSixthHarmonic")->setValueNotifyingHost(norm);
        if (cc == 78) apvts.getParameter("hammondThreeOctavesUp")->setValueNotifyingHost(norm);
        if (cc == 79) apvts.getParameter("hammondFifth")->setValueNotifyingHost(norm);
        if (cc == 80) apvts.getParameter("hammondSubOctave")->setValueNotifyingHost(norm);
        // Harmonics / Subharmonics
        if (cc == 81) apvts.getParameter("harmonics")->setValueNotifyingHost(norm);
        if (cc == 82) apvts.getParameter("subharmonics")->setValueNotifyingHost(norm);
        // ADSR
        if (cc == 84) apvts.getParameter("attack")->setValueNotifyingHost(norm);
        if (cc == 85) apvts.getParameter("decay")->setValueNotifyingHost(norm);
        if (cc == 86) apvts.getParameter("sustain")->setValueNotifyingHost(norm);
        if (cc == 87) apvts.getParameter("release")->setValueNotifyingHost(norm);
    }

    if (muted.load())
    {
        buffer.clear();
        midiMessages.clear();
        return;
    }

    juce::AudioSourceChannelInfo channelInfo(buffer);
    synthAudioSource.getNextAudioBlock(channelInfo, midiMessages);
    midiMessages.clear();

    // Soft-clip output to prevent hard clipping when many voices overlap
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* samples = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            samples[i] = std::tanh(samples[i]);
    }

    // ---- Rotary (Leslie) effect ------------------------------------------------
    if (rotaryEnabled && buffer.getNumChannels() >= 1)
    {
        const float twoPi   = juce::MathConstants<float>::twoPi;
        const float halfPi  = juce::MathConstants<float>::halfPi;
        const float hornTargetHz = rotaryFast ? 6.7f : 0.8f;
        const float drumTargetHz = rotaryFast ? 5.7f : 0.6f;
        // Ramp speed with ~0.5 s time constant
        const float rampRate = 1.0f / (0.5f * (float)currentSampleRate);

        auto* ch0 = buffer.getWritePointer(0);
        auto* ch1 = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

        // Fractional delay read with linear interpolation
        auto readDelay = [&](std::array<float, kRotaryBufSize>& buf, int wPos, float delayF) -> float
        {
            delayF = juce::jmax(2.0f, delayF);
            int   d    = (int)delayF;
            float frac = delayF - (float)d;
            int   posA = (wPos - d     + kRotaryBufSize) & (kRotaryBufSize - 1);
            int   posB = (wPos - d - 1 + kRotaryBufSize) & (kRotaryBufSize - 1);
            return buf[posA] + frac * (buf[posB] - buf[posA]);
        };

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            // Smooth speed ramp
            hornSpeed += rampRate * (hornTargetHz - hornSpeed);
            drumSpeed += rampRate * (drumTargetHz - drumSpeed);

            // Advance LFO phases
            hornAngle += hornSpeed / (float)currentSampleRate;
            if (hornAngle >= 1.0f) hornAngle -= 1.0f;
            drumAngle += drumSpeed / (float)currentSampleRate;
            if (drumAngle >= 1.0f) drumAngle -= 1.0f;

            const float hornRad = hornAngle * twoPi;
            const float drumRad = drumAngle * twoPi;

            // Mono input (synth is dual-mono)
            const float monoIn = ch0[i];

            // Crossover: one-pole LP for drum band, remainder for horn
            lpState += lpCoeff * (monoIn - lpState);
            const float drumIn = lpState;
            const float hornIn = monoIn - drumIn;

            // --- Horn (treble) ---
            hornBuffer[hornWrite] = hornIn;

            const float hDelayL = hornCenterDelay + hornDelayDepth * std::sin(hornRad);
            const float hDelayR = hornCenterDelay + hornDelayDepth * std::sin(hornRad + halfPi);

            float hornL = readDelay(hornBuffer, hornWrite, hDelayL) * (0.75f + 0.1f * std::cos(hornRad));
            float hornR = readDelay(hornBuffer, hornWrite, hDelayR) * (0.75f + 0.1f * std::cos(hornRad + halfPi));
            hornWrite = (hornWrite + 1) & (kRotaryBufSize - 1);

            // --- Drum (bass) ---
            drumBuffer[drumWrite] = drumIn;

            const float dDelayL = drumCenterDelay + drumDelayDepth * std::sin(drumRad);
            const float dDelayR = drumCenterDelay + drumDelayDepth * std::sin(drumRad + halfPi);

            float drumL = readDelay(drumBuffer, drumWrite, dDelayL) * (0.85f + 0.05f * std::cos(drumRad));
            float drumR = readDelay(drumBuffer, drumWrite, dDelayR) * (0.85f + 0.05f * std::cos(drumRad + halfPi));
            drumWrite = (drumWrite + 1) & (kRotaryBufSize - 1);

            ch0[i] = hornL + drumL;
            if (ch1) ch1[i] = hornR + drumR;
        }
    }
}

//==============================================================================
bool AudioPluginAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor(*this);
}

//==============================================================================
void AudioPluginAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Save the APVTS state as XML
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AudioPluginAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Restore the APVTS state from XML
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

//==============================================================================
void AudioPluginAudioProcessor::timerCallback()
{
    auto available = juce::MidiInput::getAvailableDevices();

    // Open any device that isn't already open
    for (auto& device : available)
    {
        bool alreadyOpen = false;
        for (auto& input : ownedMidiInputs)
            if (input->getIdentifier() == device.identifier)
                { alreadyOpen = true; break; }

        if (!alreadyOpen)
            if (auto input = juce::MidiInput::openDevice(device.identifier, this))
            {
                input->start();
                ownedMidiInputs.push_back(std::move(input));
            }
    }

    // Remove inputs whose device is no longer available
    ownedMidiInputs.erase(
        std::remove_if(ownedMidiInputs.begin(), ownedMidiInputs.end(),
            [&available](const std::unique_ptr<juce::MidiInput>& input)
            {
                for (auto& dev : available)
                    if (dev.identifier == input->getIdentifier())
                        return false;
                return true;
            }),
        ownedMidiInputs.end());
}

void AudioPluginAudioProcessor::handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& msg)
{
    midiDeviceCollector.addMessageToQueue(msg);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}