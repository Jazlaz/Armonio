#include "SynthAudioSource.h"

SynthAudioSource::SynthAudioSource(juce::MidiKeyboardState& keyState)
    : keyboardState(keyState)
{
    initializeWavetables();

    // Add voices
    for (auto i = 0; i < 16; ++i)
        synth.addVoice(new WavetableVoice());

    synth.addSound(new WavetableSound(sineWavetable));
}

void SynthAudioSource::initializeWavetables()
{
    const unsigned int tableSize = 2048;

    float referenceFreq = 261.63f;

    sineWavetable = WaveformGenerator::createSineWave(
        tableSize, 1, referenceFreq, (float)currentSampleRate);
    sawWavetable = WaveformGenerator::createSawWave(
        tableSize, 1, referenceFreq, (float)currentSampleRate);
    squareWavetable = WaveformGenerator::createSquareWave(
        tableSize, 1, referenceFreq, (float)currentSampleRate);
    triangleWavetable = WaveformGenerator::createTriangleWave(
        tableSize, 1, referenceFreq, (float)currentSampleRate);
}

void SynthAudioSource::regenerateWavetables()
{
    const unsigned int tableSize = 2048;
    float referenceFreq = 261.63f; // Middle C

    // Regenerate with current harmonic count and band-limiting
    sineWavetable = WaveformGenerator::createSineWave(
        tableSize, currentNumHarmonics, referenceFreq, (float)currentSampleRate);
    sawWavetable = WaveformGenerator::createSawWave(
        tableSize, currentNumHarmonics, referenceFreq, (float)currentSampleRate);
    squareWavetable = WaveformGenerator::createSquareWave(
        tableSize, currentNumHarmonics, referenceFreq, (float)currentSampleRate);
    triangleWavetable = WaveformGenerator::createTriangleWave(
        tableSize, currentNumHarmonics, referenceFreq, (float)currentSampleRate);

    setWaveform(currentWaveform);
}

void SynthAudioSource::setWaveform(int waveformType)
{
    currentWaveform = waveformType;
    synth.clearSounds();

    switch (waveformType)
    {
        case 0:
            synth.addSound(new WavetableSound(sineWavetable));
            break;
        case 1:
            synth.addSound(new WavetableSound(sawWavetable));
            break;
        case 2:
            synth.addSound(new WavetableSound(squareWavetable));
            break;
        case 3:
            synth.addSound(new WavetableSound(triangleWavetable));
            break;
        default:
            synth.addSound(new WavetableSound(sineWavetable));
            break;
    }
}

void SynthAudioSource::setNumHarmonics(int numHarmonics)
{
    currentNumHarmonics = juce::jlimit(1, 16, numHarmonics);
    regenerateWavetables();
}

void SynthAudioSource::setNumSubharmonics(int numSubharmonics)
{
    currentNumSubharmonics = juce::jlimit(0, 8, numSubharmonics);

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
        {
            voice->setNumSubharmonics(currentNumSubharmonics);
        }
    }
}

void SynthAudioSource::setADSRParameters(float attack, float decay, float sustain, float release)
{
    juce::ADSR::Parameters params;
    params.attack = attack;
    params.decay = decay;
    params.sustain = sustain;
    params.release = release;

    for (int i = 0; i < synth.getNumVoices(); ++i)
    {
        if (auto* voice = dynamic_cast<WavetableVoice*>(synth.getVoice(i)))
        {
            voice->setADSRParameters(params);
        }
    }
}

void SynthAudioSource::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    currentSampleRate = sampleRate;
    synth.setCurrentPlaybackSampleRate(sampleRate);

    // Regenerate wavetables with correct sample rate for band-limiting
    regenerateWavetables();
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

    synth.renderNextBlock(
        *bufferToFill.buffer,
        midiMessages,
        bufferToFill.startSample,
        bufferToFill.numSamples);
}