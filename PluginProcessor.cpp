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
    apvts.addParameterListener("subharmonics", this);  // NEW: Listen for subharmonic changes
    apvts.addParameterListener("attack", this);
    apvts.addParameterListener("decay", this);
    apvts.addParameterListener("sustain", this);
    apvts.addParameterListener("release", this);
}

AudioPluginAudioProcessor::~AudioPluginAudioProcessor()
{
    // Clean up listeners
    apvts.removeParameterListener("waveform", this);
    apvts.removeParameterListener("harmonics", this);
    apvts.removeParameterListener("subharmonics", this);  // NEW
    apvts.removeParameterListener("attack", this);
    apvts.removeParameterListener("decay", this);
    apvts.removeParameterListener("sustain", this);
    apvts.removeParameterListener("release", this);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AudioPluginAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Waveform selector (0=Sine, 1=Saw, 2=Square, 3=Triangle)
    layout.add(std::make_unique<juce::AudioParameterInt>(
        "waveform",
        "Waveform",
        0, 3, 0));

    // Harmonics
    layout.add(std::make_unique<juce::AudioParameterInt>(
        "harmonics",
        "Harmonics",
        1, 16, 1));

    // Subharmonics
    layout.add(std::make_unique<juce::AudioParameterInt>(
        "subharmonics",                // Parameter ID
        "Subharmonics",                // Parameter name
        0,                             // Min: 0 (no subharmonics)
        8,                             // Max: 8 subharmonics
        0));                           // Default: 0 (no subharmonics)

    // Attack time (1ms to 2 seconds)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "attack",
        "Attack",
        juce::NormalisableRange<float>(0.001f, 2.0f, 0.001f),
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
        synthAudioSource.setNumHarmonics((int)newValue);
    }
    else if (parameterID == "subharmonics")
    {
        synthAudioSource.setNumSubharmonics((int)newValue);
    }
    else if (parameterID == "attack" ||
             parameterID == "decay" ||
             parameterID == "sustain" ||
             parameterID == "release")
    {
        updateSynthParameters();
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
    juce::AudioSourceChannelInfo channelInfo(buffer);
    synthAudioSource.getNextAudioBlock(channelInfo, midiMessages);
    midiMessages.clear();
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
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioPluginAudioProcessor();
}