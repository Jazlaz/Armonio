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
    apvts.addParameterListener("windLevel",          this);
    apvts.addParameterListener("windThreshold",      this);
    apvts.addParameterListener("windUpperThreshold", this);
    apvts.addParameterListener("windWidth",          this);
    apvts.addParameterListener("chiffLevel",         this);
    apvts.addParameterListener("jiEnabled",          this);
    apvts.addParameterListener("jiKey",              this);
    apvts.addParameterListener("ladderEnabled",      this);
    apvts.addParameterListener("ladderCutoff",       this);
    apvts.addParameterListener("ladderResonance",    this);
    apvts.addParameterListener("ladderDrive",        this);
    apvts.addParameterListener("ladderMode",         this);
    // Start scanning for MIDI devices immediately, then every 2 seconds
    startTimer(50);   // 50ms tick: ADC every tick, MIDI scan every 40th tick
    timerCallback();

#if JUCE_LINUX
    // Open SPI devices for MCP3008 ADC reading
    // Both SPI chips owned by gpio_midi_bridge.py — C++ does no ADC reading
    spiFd0 = -1;
    spiFd1 = -1;

    auto configureSpi = [](int fd)
    {
        if (fd < 0) return;
        uint8_t mode  = SPI_MODE_0;
        uint8_t bits  = 8;
        uint32_t speed = 1000000;
        ::ioctl(fd, SPI_IOC_WR_MODE,          &mode);
        ::ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
        ::ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ,  &speed);
    };
    configureSpi(spiFd0);
    configureSpi(spiFd1);
    lastAdcRaw.fill(-1);
#endif
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
    apvts.removeParameterListener("windLevel",          this);
    apvts.removeParameterListener("windThreshold",      this);
    apvts.removeParameterListener("windUpperThreshold", this);
    apvts.removeParameterListener("windWidth",          this);
    apvts.removeParameterListener("chiffLevel",         this);
    apvts.removeParameterListener("jiEnabled",          this);
    apvts.removeParameterListener("jiKey",              this);
    apvts.removeParameterListener("ladderEnabled",      this);
    apvts.removeParameterListener("ladderCutoff",       this);
    apvts.removeParameterListener("ladderResonance",    this);
    apvts.removeParameterListener("ladderDrive",        this);
    apvts.removeParameterListener("ladderMode",         this);

    stopTimer();
    ownedMidiInputs.clear();

#if JUCE_LINUX
    if (spiFd0 >= 0) { ::close(spiFd0); spiFd0 = -1; }
    if (spiFd1 >= 0) { ::close(spiFd1); spiFd1 = -1; }
#endif
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

    // Release time (1ms to 2.5 seconds, log skew for better short-time resolution)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "release",
        "Release",
        juce::NormalisableRange<float>(0.001f, 2.5f, 0.001f, 0.35f),
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
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.5f),
        0.0f));

    // Hammond sub-octave mix level
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "hammondSubOctave",
        "Hammond Sub Octave",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.5f),
        0.0f));

    // Hammond chime buttons
    layout.add(std::make_unique<juce::AudioParameterBool>("hammondChime2", "Chime 2nd", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("hammondChime3", "Chime 3rd", false));

    // Hammond register levels
    layout.add(std::make_unique<juce::AudioParameterFloat>("hammondOctaveUp",       "Octave Up",        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.5f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("hammondThirdHarmonic",  "Oct + Fifth",      juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.5f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("hammondTwoOctavesUp",   "Two Octaves Up",   juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.5f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("hammondFifthHarmonic",  "2 Oct + Third",    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.5f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("hammondSixthHarmonic",  "2 Oct + Fifth",    juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.5f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("hammondThreeOctavesUp", "Three Octaves Up", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.5f), 0.0f));

    // Rotary (Leslie) effect
    layout.add(std::make_unique<juce::AudioParameterBool>("rotaryEnabled", "Rotary Enabled", false));
    layout.add(std::make_unique<juce::AudioParameterBool>("rotaryFast",    "Rotary Fast",    false));

    // Wind / air noise
    layout.add(std::make_unique<juce::AudioParameterInt>("windWidth", "Wind Width", 1, 32, 8));
    layout.add(std::make_unique<juce::AudioParameterFloat>("windLevel", "Wind Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.35f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("chiffLevel", "Chiff Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f, 0.35f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("windThreshold", "Wind Threshold",
        juce::NormalisableRange<float>(500.0f, 10000.0f,
            [](float s, float e, float n) { return s * std::pow(e / s, n); },
            [](float s, float e, float v) { return std::log(v / s) / std::log(e / s); },
            nullptr),
        1000.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("windUpperThreshold", "Wind Upper Threshold",
        juce::NormalisableRange<float>(1000.0f, 20000.0f,
            [](float s, float e, float n) { return s * std::pow(e / s, n); },
            [](float s, float e, float v) { return std::log(v / s) / std::log(e / s); },
            nullptr),
        20000.0f));

    layout.add(std::make_unique<juce::AudioParameterBool>("jiEnabled", "JI Enabled", false));
    layout.add(std::make_unique<juce::AudioParameterInt>("jiKey", "JI Root Key", 0, 11, 0));

    // Ladder filter
    layout.add(std::make_unique<juce::AudioParameterBool>("ladderEnabled", "Ladder Enabled", false));
    layout.add(std::make_unique<juce::AudioParameterInt>("ladderMode", "Ladder Mode", 0, 5, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>("ladderCutoff", "Ladder Cutoff",
        juce::NormalisableRange<float>(20.0f, 20000.0f,
            [](float s, float e, float n) { return s * std::pow(e / s, n); },
            [](float s, float e, float v) { return std::log(v / s) / std::log(e / s); },
            nullptr),
        5000.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("ladderResonance", "Ladder Resonance",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("ladderDrive", "Ladder Drive",
        juce::NormalisableRange<float>(1.0f, 25.0f, 0.01f, 0.35f), 1.0f));

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
    else if (parameterID == "rotaryEnabled") { rotaryEnabled.store(newValue > 0.5f); }
    else if (parameterID == "rotaryFast")    { rotaryFast.store(newValue > 0.5f); }
    else if (parameterID == "windLevel")          { synthAudioSource.setWindLevel(newValue); }
    else if (parameterID == "windThreshold")      { synthAudioSource.setWindThreshold(newValue); }
    else if (parameterID == "windUpperThreshold") { synthAudioSource.setWindUpperThreshold(newValue); }
    else if (parameterID == "windWidth")          { synthAudioSource.setWindWidth(juce::roundToInt(newValue)); }
    else if (parameterID == "chiffLevel")         { synthAudioSource.setChiffLevel(newValue); }
    else if (parameterID == "jiEnabled")          { synthAudioSource.setJIEnabled(newValue > 0.5f); }
    else if (parameterID == "jiKey")              { synthAudioSource.setJIRootKey(juce::roundToInt(newValue)); }
    else if (parameterID == "ladderEnabled")      { ladderEnabled.store(newValue > 0.5f); }
    else if (parameterID == "ladderCutoff")       { ladderFilter.setCutoffFrequencyHz(newValue); }
    else if (parameterID == "ladderResonance")    { ladderFilter.setResonance(newValue); }
    else if (parameterID == "ladderDrive")        { ladderFilter.setDrive(newValue); }
    else if (parameterID == "ladderMode")
    {
        using Mode = juce::dsp::LadderFilter<float>::Mode;
        static constexpr Mode modes[] = {
            Mode::LPF12, Mode::LPF24, Mode::HPF12, Mode::HPF24, Mode::BPF12, Mode::BPF24
        };
        ladderFilter.setMode(modes[juce::jlimit(0, 5, juce::roundToInt(newValue))]);
    }
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
    lpCoeff       = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 800.0f  / (float)sampleRate);

    hornBuffer.fill(0.0f);
    drumBuffer.fill(0.0f);
    hornWrite = drumWrite = 0;
    lpState   = 0.0f;
    sHorn = sDrum = 0.0f;
    cHorn = cDrum = 1.0f;
    hornSpeed = rotaryFast.load() ? 6.7f : 0.8f;
    drumSpeed = rotaryFast.load() ? 5.7f : 0.6f;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32)samplesPerBlock;
    spec.numChannels      = (juce::uint32)getTotalNumOutputChannels();
    ladderFilter.prepare(spec);
    ladderFilter.setCutoffFrequencyHz(apvts.getRawParameterValue("ladderCutoff")->load());
    ladderFilter.setResonance(apvts.getRawParameterValue("ladderResonance")->load());
    ladderFilter.setDrive(apvts.getRawParameterValue("ladderDrive")->load());

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

    // Merge directly-opened MIDI devices into the buffer.
    // Only on Linux (Pi): JUCE's AudioDeviceManager is not available headless,
    // so we open devices manually. On Windows the DAW or JUCE standalone wrapper
    // already handles device routing; merging here would duplicate every note.
  #if JUCE_LINUX
    midiDeviceCollector.removeNextBlockOfMessages(midiMessages, buffer.getNumSamples());
  #endif

    // Minilab MK2 MIDI CC mapping. Single pass also filters out Note Ons
    // that arrive while the JI gate (CC 32) is held — those become root-key
    // selections rather than playing a note.
    juce::MidiBuffer filteredMidi;
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();

        // ── JI gate: capture the next Note On as the new root, swallow it. ──
        if (msg.isNoteOn() && jiButtonHeld)
        {
            const int pitchClass = msg.getNoteNumber() % 12;
            if (auto* p = apvts.getParameter("jiKey"))
                p->setValueNotifyingHost(p->convertTo0to1((float)pitchClass));
            if (auto* e = apvts.getParameter("jiEnabled"))
                e->setValueNotifyingHost(1.0f);
            jiNotePickedDuringHold = true;
            continue;   // do NOT add to filteredMidi — note is consumed
        }

        if (!msg.isController())
        {
            filteredMidi.addEvent(msg, metadata.samplePosition);
            continue;
        }

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

        // Mode 2 buttons: LFO enable / Filter enable+mode / JI enable
        if (cc == 28) apvts.getParameter("lfoEnabled")->setValueNotifyingHost(val > 0 ? 1.0f : 0.0f);
        if (cc == 29) apvts.getParameter("ladderEnabled")->setValueNotifyingHost(val > 0 ? 1.0f : 0.0f);
        if (cc == 30)
        {
            // Each press steps forward through 6 filter modes (LP12/LP24/HP12/HP24/BP12/BP24).
            // Val>0 = forward, val==0 = unused (GPIO Python sends 127 on press).
            if (val > 0)
            {
                if (auto* p = apvts.getParameter("ladderMode"))
                {
                    const int cur  = juce::roundToInt(p->convertFrom0to1(p->getValue()));
                    const int next = (cur + 1) % 6;
                    p->setValueNotifyingHost(p->convertTo0to1((float)next));
                }
            }
        }
        if (cc == 31)   // backward through filter modes
        {
            if (val > 0)
            {
                if (auto* p = apvts.getParameter("ladderMode"))
                {
                    const int cur  = juce::roundToInt(p->convertFrom0to1(p->getValue()));
                    const int prev = (cur + 5) % 6;
                    p->setValueNotifyingHost(p->convertTo0to1((float)prev));
                }
            }
        }
        if (cc == 32)
        {
            // Gate semantics: 127 = press, 0 = release.
            // Tap with no note in between toggles jiEnabled.
            // Hold + Note On is handled at the top of the loop.
            const bool pressed = val >= 64;
            if (pressed)
            {
                jiButtonHeld          = true;
                jiNotePickedDuringHold = false;
            }
            else
            {
                jiButtonHeld = false;
                if (! jiNotePickedDuringHold)
                {
                    auto* p = apvts.getParameter("jiEnabled");
                    p->setValueNotifyingHost(p->getValue() > 0.5f ? 0.0f : 1.0f);
                }
            }
        }

        // CC 90: knob mode selector from Python GPIO script (0=Mode1, 64=Mode2, 127=Mode3).
        // Stores the ADC-routing mode; timerCallback uses it to map pot positions.
        if (cc == 90)
        {
            knobMode.store(val < 32 ? 0 : val < 96 ? 1 : 2);
        }

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
        if (cc == 81) apvts.getParameter("subharmonics")->setValueNotifyingHost(norm);
        if (cc == 82) apvts.getParameter("windThreshold")->setValueNotifyingHost(norm);
        if (cc == 83) apvts.getParameter("windLevel")->setValueNotifyingHost(norm);
        if (cc == 88) apvts.getParameter("harmonics")->setValueNotifyingHost(norm);
        // ADSR
        if (cc == 84) apvts.getParameter("attack")->setValueNotifyingHost(norm);
        if (cc == 85) apvts.getParameter("decay")->setValueNotifyingHost(norm);
        if (cc == 86) apvts.getParameter("sustain")->setValueNotifyingHost(norm);
        if (cc == 87) apvts.getParameter("release")->setValueNotifyingHost(norm);

        // Mode-2 knobs sent by gpio bridge: filter params + air high-cut
        if (cc == 91) apvts.getParameter("ladderCutoff")->setValueNotifyingHost(norm);
        if (cc == 92) apvts.getParameter("ladderResonance")->setValueNotifyingHost(norm);
        if (cc == 93) apvts.getParameter("ladderDrive")->setValueNotifyingHost(norm);
        if (cc == 94) apvts.getParameter("windUpperThreshold")->setValueNotifyingHost(norm);

        // Controllers themselves are inert at the synth (controllerMoved is empty),
        // but we keep them in the buffer for any downstream consumer (host MIDI thru,
        // editor display).
        filteredMidi.addEvent(msg, metadata.samplePosition);
    }
    midiMessages.swapWith(filteredMidi);

    juce::AudioSourceChannelInfo channelInfo(buffer);
    synthAudioSource.getNextAudioBlock(channelInfo, midiMessages);
    midiMessages.clear();

    // Soft-clip + mute + Leslie are fused into a single per-sample loop.
    // Soft clip uses a Padé-style tanh approximation: smooth everywhere, ~6 cycles
    // instead of a 30-50 cycle glibc tanhf on Cortex-A53.
    const float rotaryTarget = rotaryEnabled.load() ? 1.0f : 0.0f;
    const float muteTarget   = muted.load() ? 0.0f : 1.0f;
    if (buffer.getNumChannels() >= 1)
    {
        const float twoPi   = juce::MathConstants<float>::twoPi;
        const float hornTargetHz = rotaryFast.load() ? 6.7f : 0.8f;
        const float drumTargetHz = rotaryFast.load() ? 5.7f : 0.6f;
        // Ramp speed with ~0.5 s time constant
        const float rampRate     = 1.0f / (0.5f * (float)currentSampleRate);
        // Crossfade ramp ~20ms
        const float gainRampStep = 1.0f / (0.02f * (float)currentSampleRate);
        // Mute ramp ~10ms
        const float muteRampStep = 1.0f / (0.01f * (float)currentSampleRate);

        // Rotation-recursion step: sin/cos of the per-sample phase advance.
        // Computed once per block (speed ramps slowly enough that freezing
        // these for one block is inaudible — <1% drift at any reasonable block size).
        // Per-sample trig calls drop from 8 to 0; the setup cost is 4 trig calls per block.
        const float hornOmega  = hornSpeed * twoPi / (float)currentSampleRate;
        const float drumOmega  = drumSpeed * twoPi / (float)currentSampleRate;
        const float cDeltaHorn = std::cos(hornOmega);
        const float sDeltaHorn = std::sin(hornOmega);
        const float cDeltaDrum = std::cos(drumOmega);
        const float sDeltaDrum = std::sin(drumOmega);

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
            // Smooth speed ramp (affects next block's cDelta/sDelta, not this one)
            hornSpeed += rampRate * (hornTargetHz - hornSpeed);
            drumSpeed += rampRate * (drumTargetHz - drumSpeed);

            // Rotation recursion: advance sin/cos pairs by one sample, no trig calls.
            // Identities used below: sin(θ+π/2) = cos(θ), cos(θ+π/2) = -sin(θ).
            const float newSHorn = sHorn * cDeltaHorn + cHorn * sDeltaHorn;
            const float newCHorn = cHorn * cDeltaHorn - sHorn * sDeltaHorn;
            sHorn = newSHorn;
            cHorn = newCHorn;
            const float newSDrum = sDrum * cDeltaDrum + cDrum * sDeltaDrum;
            const float newCDrum = cDrum * cDeltaDrum - sDrum * sDeltaDrum;
            sDrum = newSDrum;
            cDrum = newCDrum;

            // Read synth output, soft-clip, then apply the mute ramp —
            // all before Leslie sees the signal.
            float monoIn = ch0[i];
            {
                const float x  = juce::jlimit(-3.0f, 3.0f, monoIn);
                const float x2 = x * x;
                monoIn = x * (27.0f + x2) / (27.0f + 9.0f * x2);
            }

            if (muteGain < muteTarget)
                muteGain = juce::jmin(muteTarget, muteGain + muteRampStep);
            else if (muteGain > muteTarget)
                muteGain = juce::jmax(muteTarget, muteGain - muteRampStep);
            monoIn *= muteGain;

            // Crossover: one-pole LP for drum band, remainder for horn
            lpState += lpCoeff * (monoIn - lpState);
            const float drumIn = lpState;
            const float hornIn = monoIn - drumIn;

            // --- Horn (treble) ---
            hornBuffer[hornWrite] = hornIn;

            const float hDelayL = hornCenterDelay + hornDelayDepth * sHorn;
            const float hDelayR = hornCenterDelay + hornDelayDepth * cHorn;   // sin(θ+π/2) = cos θ

            float hornL = readDelay(hornBuffer, hornWrite, hDelayL) * (0.75f + 0.1f *  cHorn);
            float hornR = readDelay(hornBuffer, hornWrite, hDelayR) * (0.75f + 0.1f * -sHorn); // cos(θ+π/2) = -sin θ
            hornWrite = (hornWrite + 1) & (kRotaryBufSize - 1);

            // --- Drum (bass) ---
            drumBuffer[drumWrite] = drumIn;

            const float dDelayL = drumCenterDelay + drumDelayDepth * sDrum;
            const float dDelayR = drumCenterDelay + drumDelayDepth * cDrum;

            float drumL = readDelay(drumBuffer, drumWrite, dDelayL) * (0.85f + 0.05f *  cDrum);
            float drumR = readDelay(drumBuffer, drumWrite, dDelayR) * (0.85f + 0.05f * -sDrum);
            drumWrite = (drumWrite + 1) & (kRotaryBufSize - 1);

            // Smooth rotaryGain toward target
            if (rotaryGain < rotaryTarget)
                rotaryGain = juce::jmin(rotaryTarget, rotaryGain + gainRampStep);
            else if (rotaryGain > rotaryTarget)
                rotaryGain = juce::jmax(rotaryTarget, rotaryGain - gainRampStep);

            // Crossfade dry <-> wet
            const float wetL = hornL + drumL;
            const float wetR = hornR + drumR;
            ch0[i] = monoIn * (1.0f - rotaryGain) + wetL * rotaryGain;
            if (ch1) ch1[i] = monoIn * (1.0f - rotaryGain) + wetR * rotaryGain;
        }

        // Rotation-recursion renormalization: float drift pulls the (sin,cos)
        // pair off the unit circle over time. Cheap Newton step: for mag² close
        // to 1, 1/sqrt(mag²) ≈ 0.5·(3 - mag²). Done once per block.
        {
            const float mh = 0.5f * (3.0f - (sHorn * sHorn + cHorn * cHorn));
            sHorn *= mh; cHorn *= mh;
            const float md = 0.5f * (3.0f - (sDrum * sDrum + cDrum * cDrum));
            sDrum *= md; cDrum *= md;
        }
    }

    // Ladder filter — applied as a stereo block after all other FX
    if (ladderEnabled.load())
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        ladderFilter.process(context);
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
#if JUCE_LINUX
int AudioPluginAudioProcessor::readMCP3008 (int fd, int channel) noexcept
{
    if (fd < 0 || channel < 0 || channel > 7)
        return 0;

    uint8_t tx[3] = { 1, (uint8_t)((8 + channel) << 4), 0 };
    uint8_t rx[3] = { 0, 0, 0 };

    struct spi_ioc_transfer tr{};
    tr.tx_buf        = (unsigned long)tx;
    tr.rx_buf        = (unsigned long)rx;
    tr.len           = 3;
    tr.speed_hz      = 1000000;
    tr.bits_per_word = 8;

    if (::ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 0)
        return 0;

    return ((rx[1] & 3) << 8) | rx[2];   // 10-bit result, 0–1023
}
#endif

void AudioPluginAudioProcessor::timerCallback()
{
#if JUCE_LINUX
    // --- ADC polling: every tick (~50ms) ---
    // Two 8-channel MCP3008 chips give 16 pots total.
    // Chip 0 ch0-7 = 8 Hammond drawbars (fixed, always Mode 1 pots).
    // Chip 1 ch0-7 = function pots whose targets switch with knobMode.
    struct AdcEntry { int chip; int ch; const char* paramId; };

    // Chip 0: Hammond drawbars — always mapped, all modes.
    static constexpr AdcEntry adcMapChip0[] = {
        { 0, 0, "hammondOctaveUp"       },
        { 0, 1, "hammondThirdHarmonic"  },
        { 0, 2, "hammondTwoOctavesUp"   },
        { 0, 3, "hammondFifthHarmonic"  },
        { 0, 4, "hammondSixthHarmonic"  },
        { 0, 5, "hammondThreeOctavesUp" },
        { 0, 6, "hammondFifth"          },
        { 0, 7, "hammondSubOctave"      },
    };

    // Chip 1: mode-dependent targets.
    // Mode 0 (default): ADSR + harmonics/subharmonics.
    static constexpr AdcEntry adcMapMode0[] = {
        { 1, 0, "attack"       },
        { 1, 1, "decay"        },
        { 1, 2, "sustain"      },
        { 1, 3, "release"      },
        { 1, 4, "harmonics"    },
        { 1, 5, "subharmonics" },
        { 1, 6, "windLevel"    },
        { 1, 7, "windThreshold"},
    };

    // Mode 1: Ladder filter + air parameters.
    static constexpr AdcEntry adcMapMode1[] = {
        { 1, 0, "ladderCutoff"        },
        { 1, 1, "ladderResonance"     },
        { 1, 2, "ladderDrive"         },
        { 1, 3, "windUpperThreshold"  },
        { 1, 4, "windLevel"           },
        { 1, 5, "windThreshold"       },
        { 1, 6, "subharmonics"        },
        { 1, 7, "harmonics"           },
    };

    // Mode 2: ADSR + chiff + harmonics (reserved for future expansion).
    static constexpr AdcEntry adcMapMode2[] = {
        { 1, 0, "attack"       },
        { 1, 1, "decay"        },
        { 1, 2, "sustain"      },
        { 1, 3, "release"      },
        { 1, 4, "harmonics"    },
        { 1, 5, "subharmonics" },
        { 1, 6, "chiffLevel"   },
        { 1, 7, "windLevel"    },
    };

    const int fds[2] = { spiFd0, spiFd1 };
    const int mode   = knobMode.load();

    // If the mode just changed, invalidate the chip-1 ADC cache so every pot
    // fires immediately on this tick and jumps to its current physical position.
    if (mode != lastKnobMode)
    {
        for (int i = 8; i < 16; ++i)
            lastAdcRaw[i] = -1;
        lastKnobMode = mode;
    }

    // Poll chip 0 (Hammond drawbars) — indices 0-7 in lastAdcRaw
    for (int i = 0; i < (int)std::size(adcMapChip0); ++i)
    {
        const auto& entry = adcMapChip0[i];
        int fd = fds[entry.chip];
        if (fd < 0) continue;
        int raw = readMCP3008(fd, entry.ch);
        if (std::abs(raw - lastAdcRaw[i]) > kAdcThreshold)
        {
            lastAdcRaw[i] = raw;
            float norm = (float)raw / 1023.0f;
            if (auto* param = apvts.getParameter(entry.paramId))
                param->setValueNotifyingHost(norm);
        }
    }

    // Poll chip 1 (mode-dependent) — indices 8-15 in lastAdcRaw
    const AdcEntry* modeMap = (mode == 1) ? adcMapMode1
                            : (mode == 2) ? adcMapMode2
                                          : adcMapMode0;
    static constexpr int kChip1Slots = 8;
    for (int i = 0; i < kChip1Slots; ++i)
    {
        const auto& entry = modeMap[i];
        int fd = fds[entry.chip];
        if (fd < 0) continue;
        const int rawIdx = 8 + i;   // offset into lastAdcRaw to avoid chip-0 slots
        int raw = readMCP3008(fd, entry.ch);
        if (std::abs(raw - lastAdcRaw[rawIdx]) > kAdcThreshold)
        {
            lastAdcRaw[rawIdx] = raw;
            float norm = (float)raw / 1023.0f;
            if (auto* param = apvts.getParameter(entry.paramId))
                param->setValueNotifyingHost(norm);
        }
    }
#endif

  #if JUCE_LINUX
    // --- MIDI device scan: every 40 ticks (~2 seconds) ---
    // Pi-only: JUCE's AudioDeviceManager isn't used headless, so we open devices
    // ourselves. On Windows (VST3 or Standalone) the host/wrapper handles this.
    if (++timerTickCount < 40)
        return;
    timerTickCount = 0;

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

    // Send all-notes-off for any device that disappeared before removing it
    for (auto& input : ownedMidiInputs)
    {
        bool stillAvailable = false;
        for (auto& dev : available)
            if (dev.identifier == input->getIdentifier())
                { stillAvailable = true; break; }

        if (!stillAvailable)
        {
            for (int ch = 1; ch <= 16; ++ch)
            {
                midiDeviceCollector.addMessageToQueue(juce::MidiMessage::allNotesOff(ch));
                midiDeviceCollector.addMessageToQueue(
                    juce::MidiMessage::controllerEvent(ch, 120, 0)); // all sound off
            }
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
  #endif // JUCE_LINUX
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