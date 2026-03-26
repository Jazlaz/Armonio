#include "SynthAudioSource.h"

SynthAudioSource::SynthAudioSource(juce::MidiKeyboardState& keyState)
    : keyboardState(keyState)
{
    for (auto i = 0; i < 12; ++i)
        synth.addVoice(new WavetableVoice());

    initializeWavetables();
    synth.addSound(new WavetableSound(sineWavetable));

    // Push defaults to all voices
    pushStateToVoices();
}

void SynthAudioSource::initializeWavetables()
{
    const unsigned int tableSize = 2048;
    sineWavetable = WaveformGenerator::createSineWave(tableSize, (float)currentSampleRate);
}

void SynthAudioSource::pushStateToVoices()
{
    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
        {
            voice->setWaveform(currentWaveform);
            voice->setNumHarmonics(currentNumHarmonics);
            voice->setNumSubharmonics(currentNumSubharmonics);
            voice->setLFODepth(currentLFODepth);
            voice->setLFOSpeed(currentLFOSpeed);
            voice->setLFOEnabled(currentLFOEnabled);
            voice->setLFOMode(currentLFOMode);
            voice->setHammondFifth(currentHammondFifth);
            voice->setHammondSubOctave(currentHammondSubOctave);
            voice->setHammondOctaveUp(currentHammondOctaveUp);
            voice->setHammondThirdHarmonic(currentHammondThirdHarmonic);
            voice->setHammondTwoOctavesUp(currentHammondTwoOctavesUp);
            voice->setHammondFifthHarmonic(currentHammondFifthHarmonic);
            voice->setHammondSixthHarmonic(currentHammondSixthHarmonic);
            voice->setHammondThreeOctavesUp(currentHammondThreeOctavesUp);
            voice->setChime2Enabled(currentChime2Enabled);
            voice->setChime3Enabled(currentChime3Enabled);
        }
    }
}

void SynthAudioSource::setWaveform(int waveformType)
{
    currentWaveform = waveformType;
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setWaveform(currentWaveform);
}

void SynthAudioSource::setNumHarmonics(float numHarmonics)
{
    currentNumHarmonics = juce::jlimit(1.0f, 16.0f, numHarmonics);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setNumHarmonics(currentNumHarmonics);
}

void SynthAudioSource::setNumSubharmonics(float numSubharmonics)
{
    currentNumSubharmonics = juce::jlimit(0.0f, 8.0f, numSubharmonics);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setNumSubharmonics(currentNumSubharmonics);
}

void SynthAudioSource::setHammondFifth(float level)
{
    currentHammondFifth = juce::jlimit(0.0f, 1.0f, level);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setHammondFifth(currentHammondFifth);
}

void SynthAudioSource::setHammondSubOctave(float level)
{
    currentHammondSubOctave = juce::jlimit(0.0f, 1.0f, level);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setHammondSubOctave(currentHammondSubOctave);
}

void SynthAudioSource::setHammondOctaveUp(float level)
{
    currentHammondOctaveUp = juce::jlimit(0.0f, 1.0f, level);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setHammondOctaveUp(currentHammondOctaveUp);
}

void SynthAudioSource::setHammondThirdHarmonic(float level)
{
    currentHammondThirdHarmonic = juce::jlimit(0.0f, 1.0f, level);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setHammondThirdHarmonic(currentHammondThirdHarmonic);
}

void SynthAudioSource::setHammondTwoOctavesUp(float level)
{
    currentHammondTwoOctavesUp = juce::jlimit(0.0f, 1.0f, level);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setHammondTwoOctavesUp(currentHammondTwoOctavesUp);
}

void SynthAudioSource::setHammondFifthHarmonic(float level)
{
    currentHammondFifthHarmonic = juce::jlimit(0.0f, 1.0f, level);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setHammondFifthHarmonic(currentHammondFifthHarmonic);
}

void SynthAudioSource::setHammondSixthHarmonic(float level)
{
    currentHammondSixthHarmonic = juce::jlimit(0.0f, 1.0f, level);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setHammondSixthHarmonic(currentHammondSixthHarmonic);
}

void SynthAudioSource::setHammondThreeOctavesUp(float level)
{
    currentHammondThreeOctavesUp = juce::jlimit(0.0f, 1.0f, level);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setHammondThreeOctavesUp(currentHammondThreeOctavesUp);
}

void SynthAudioSource::setChime2Enabled(bool enabled)
{
    currentChime2Enabled = enabled;
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setChime2Enabled(currentChime2Enabled);
}

void SynthAudioSource::setChime3Enabled(bool enabled)
{
    currentChime3Enabled = enabled;
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setChime3Enabled(currentChime3Enabled);
}

void SynthAudioSource::setLFOEnabled(bool enabled)
{
    currentLFOEnabled = enabled;
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setLFOEnabled(currentLFOEnabled);
}

void SynthAudioSource::setLFOMode(int mode)
{
    currentLFOMode = juce::jlimit(0, 4, mode);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setLFOMode(currentLFOMode);
}

void SynthAudioSource::setLFODepth(float depth)
{
    currentLFODepth = juce::jlimit(0.0f, 0.05f, depth);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setLFODepth(currentLFODepth);
}

void SynthAudioSource::setLFOSpeed(float speed)
{
    currentLFOSpeed = juce::jlimit(0.001f, 0.5f, speed);
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setLFOSpeed(currentLFOSpeed);
}

void SynthAudioSource::setADSRParameters(float attack, float decay, float sustain, float release)
{
    juce::ADSR::Parameters params;
    params.attack  = attack;
    params.decay   = decay;
    params.sustain = sustain;
    params.release = release;

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
            voice->setADSRParameters(params);
}

void SynthAudioSource::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused(samplesPerBlockExpected);

    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate(sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = synth.getVoice(i))
            voice->setCurrentPlaybackSampleRate(sampleRate);

    // Rebuild the pure sine table with the correct sample rate
    initializeWavetables();
    synth.clearSounds();
    synth.addSound(new WavetableSound(sineWavetable));

    pushStateToVoices();
}

void SynthAudioSource::releaseResources()
{
}

void SynthAudioSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    juce::MidiBuffer emptyMidiBuffer;
    getNextAudioBlock(bufferToFill, emptyMidiBuffer);
}

void SynthAudioSource::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill,
                                         juce::MidiBuffer& midiMessages)
{
    bufferToFill.clearActiveBufferRegion();

    keyboardState.processNextMidiBuffer(
        midiMessages,
        bufferToFill.startSample,
        bufferToFill.numSamples,
        true);

    juce::MidiBuffer filteredMidi;

    for (const auto metadata : midiMessages)
    {
        const auto& msg = metadata.getMessage();

        if (!msg.isSysEx() &&
            !msg.isMidiClock() &&
            !msg.isMidiStart() &&
            !msg.isMidiStop() &&
            !msg.isMidiContinue() &&
            !msg.isActiveSense() &&
            !msg.isQuarterFrame())
        {
            filteredMidi.addEvent(msg, metadata.samplePosition);
        }
    }

    synth.renderNextBlock(
        *bufferToFill.buffer,
        filteredMidi,
        bufferToFill.startSample,
        bufferToFill.numSamples);
}
