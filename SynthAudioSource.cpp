#include "SynthAudioSource.h"

SynthAudioSource::SynthAudioSource(juce::MidiKeyboardState& keyState)
    : keyboardState(keyState)
{
    for (int i = 0; i < kNumVoices; ++i)
    {
        auto* v = new WavetableVoice();
        // The last kNumFadeVoices voices are reserved for release-tail handoff.
        // HammondSynth skips them in findFreePrimary + findVoiceToSteal, so
        // new notes never land on them and they never get stolen.
        if (i >= kNumPrimaryVoices)
            v->setFadeVoice(true);
        synth.addVoice(v);
        voices[i] = v;
    }

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
    for (auto* voice : voices)
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
        voice->setWindLevel(currentWindLevel);
        voice->setWindThreshold(currentWindThreshold);
        voice->setWindUpperThreshold(currentWindUpperThreshold);
        voice->setWindWidth(currentWindWidth);
        voice->setChiffLevel(currentChiffLevel);
        voice->setJIEnabled(currentJIEnabled);
        voice->setJIRootKey(currentJIRootKey);
    }
}

void SynthAudioSource::setWaveform(int waveformType)
{
    currentWaveform = waveformType;
    for (auto* v : voices) v->setWaveform(currentWaveform);
}

void SynthAudioSource::setNumHarmonics(float numHarmonics)
{
    currentNumHarmonics = juce::jlimit(1.0f, 16.0f, numHarmonics);
    for (auto* v : voices) v->setNumHarmonics(currentNumHarmonics);
}

void SynthAudioSource::setNumSubharmonics(float numSubharmonics)
{
    currentNumSubharmonics = juce::jlimit(0.0f, 8.0f, numSubharmonics);
    for (auto* v : voices) v->setNumSubharmonics(currentNumSubharmonics);
}

void SynthAudioSource::setHammondFifth(float level)
{
    currentHammondFifth = juce::jlimit(0.0f, 1.0f, level);
    for (auto* v : voices) v->setHammondFifth(currentHammondFifth);
}

void SynthAudioSource::setHammondSubOctave(float level)
{
    currentHammondSubOctave = juce::jlimit(0.0f, 1.0f, level);
    for (auto* v : voices) v->setHammondSubOctave(currentHammondSubOctave);
}

void SynthAudioSource::setHammondOctaveUp(float level)
{
    currentHammondOctaveUp = juce::jlimit(0.0f, 1.0f, level);
    for (auto* v : voices) v->setHammondOctaveUp(currentHammondOctaveUp);
}

void SynthAudioSource::setHammondThirdHarmonic(float level)
{
    currentHammondThirdHarmonic = juce::jlimit(0.0f, 1.0f, level);
    for (auto* v : voices) v->setHammondThirdHarmonic(currentHammondThirdHarmonic);
}

void SynthAudioSource::setHammondTwoOctavesUp(float level)
{
    currentHammondTwoOctavesUp = juce::jlimit(0.0f, 1.0f, level);
    for (auto* v : voices) v->setHammondTwoOctavesUp(currentHammondTwoOctavesUp);
}

void SynthAudioSource::setHammondFifthHarmonic(float level)
{
    currentHammondFifthHarmonic = juce::jlimit(0.0f, 1.0f, level);
    for (auto* v : voices) v->setHammondFifthHarmonic(currentHammondFifthHarmonic);
}

void SynthAudioSource::setHammondSixthHarmonic(float level)
{
    currentHammondSixthHarmonic = juce::jlimit(0.0f, 1.0f, level);
    for (auto* v : voices) v->setHammondSixthHarmonic(currentHammondSixthHarmonic);
}

void SynthAudioSource::setHammondThreeOctavesUp(float level)
{
    currentHammondThreeOctavesUp = juce::jlimit(0.0f, 1.0f, level);
    for (auto* v : voices) v->setHammondThreeOctavesUp(currentHammondThreeOctavesUp);
}

void SynthAudioSource::setChime2Enabled(bool enabled)
{
    currentChime2Enabled = enabled;
    for (auto* v : voices) v->setChime2Enabled(currentChime2Enabled);
}

void SynthAudioSource::setWindLevel(float level)
{
    currentWindLevel = juce::jlimit(0.0f, 1.0f, level);
    for (auto* v : voices) v->setWindLevel(currentWindLevel);
}

void SynthAudioSource::setWindThreshold(float freqHz)
{
    currentWindThreshold = freqHz;
    for (auto* v : voices) v->setWindThreshold(currentWindThreshold);
}

void SynthAudioSource::setWindUpperThreshold(float freqHz)
{
    currentWindUpperThreshold = freqHz;
    for (auto* v : voices) v->setWindUpperThreshold(currentWindUpperThreshold);
}

void SynthAudioSource::setWindWidth(int n)
{
    currentWindWidth = n;
    for (auto* v : voices) v->setWindWidth(currentWindWidth);
}

void SynthAudioSource::setChiffLevel(float level)
{
    currentChiffLevel = juce::jlimit(0.0f, 1.0f, level);
    for (auto* v : voices) v->setChiffLevel(currentChiffLevel);
}

void SynthAudioSource::setChime3Enabled(bool enabled)
{
    currentChime3Enabled = enabled;
    for (auto* v : voices) v->setChime3Enabled(currentChime3Enabled);
}

void SynthAudioSource::setLFOEnabled(bool enabled)
{
    currentLFOEnabled = enabled;
    for (auto* v : voices) v->setLFOEnabled(currentLFOEnabled);
}

void SynthAudioSource::setLFOMode(int mode)
{
    currentLFOMode = juce::jlimit(0, 4, mode);
    for (auto* v : voices) v->setLFOMode(currentLFOMode);
}

void SynthAudioSource::setLFODepth(float depth)
{
    currentLFODepth = juce::jlimit(0.0f, 0.05f, depth);
    for (auto* v : voices) v->setLFODepth(currentLFODepth);
}

void SynthAudioSource::setLFOSpeed(float speed)
{
    currentLFOSpeed = juce::jlimit(0.001f, 0.5f, speed);
    for (auto* v : voices) v->setLFOSpeed(currentLFOSpeed);
}

void SynthAudioSource::setADSRParameters(float attack, float decay, float sustain, float release)
{
    juce::ADSR::Parameters params;
    params.attack  = attack;
    params.decay   = decay;
    params.sustain = sustain;
    params.release = release;

    for (auto* v : voices) v->setADSRParameters(params);
}

void SynthAudioSource::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused(samplesPerBlockExpected);

    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate(sampleRate);

    for (auto* v : voices)
        v->setCurrentPlaybackSampleRate(sampleRate);

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

    // Keep voice count ≤ 10: fade out the longest-releasing tail if over the limit.
    trimExcessVoices();

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

void SynthAudioSource::setJIEnabled(bool enabled)
{
    currentJIEnabled = enabled;
    for (auto* v : voices) v->setJIEnabled(currentJIEnabled);
}

void SynthAudioSource::setJIRootKey(int key)
{
    currentJIRootKey = juce::jlimit(0, 11, key);
    for (auto* v : voices) v->setJIRootKey(currentJIRootKey);
}
