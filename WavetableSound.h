#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "WavetableOscillator.h"

//==============================================================================
class WavetableSound : public juce::SynthesiserSound
{
public:
    WavetableSound(const juce::AudioSampleBuffer& wavetableToUse)
        : wavetable(wavetableToUse)
    {
    }

    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }

    const juce::AudioSampleBuffer& getWavetable() const { return wavetable; }

private:
    juce::AudioSampleBuffer wavetable;
};

//==============================================================================
class WavetableVoice : public juce::SynthesiserVoice
{
public:
    WavetableVoice()
    {
        adsr.setSampleRate(44100.0);

        juce::ADSR::Parameters params;
        params.attack = 0.01f;
        params.decay = 0.1f;
        params.sustain = 0.8f;
        params.release = 0.3f;
        adsr.setParameters(params);
    }

    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<WavetableSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound* sound,
                   int /*currentPitchWheelPosition*/) override
    {
        auto* wavetableSound = dynamic_cast<WavetableSound*>(sound);
        if (wavetableSound != nullptr)
        {
            auto sampleRate = getSampleRate();
            auto fundamentalFreq = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);

            // Create main oscillator with random starting phase
            mainOscillator = std::make_unique<WavetableOscillator>(wavetableSound->getWavetable());
            mainOscillator->setFrequency((float)fundamentalFreq, (float)sampleRate);
            mainOscillator->setRandomPhase();

            // Calculate Nyquist frequency for band-limiting
            float nyquistFreq = (float)sampleRate / 2.0f;

            // Create subharmonic oscillators with band-limiting
            subharmonicOscillators.clear();
            for (int i = 0; i < numSubharmonics; ++i)
            {
                float divisor = (float)(i + 2);
                float subFreq = fundamentalFreq / divisor;

                if (subFreq < nyquistFreq)
                {
                    auto subOsc = std::make_unique<WavetableOscillator>(wavetableSound->getWavetable());
                    subOsc->setFrequency(subFreq, (float)sampleRate);
                    subOsc->setRandomPhase();
                    subharmonicOscillators.push_back(std::move(subOsc));
                }
            }

            level = velocity * 0.15f;
            tailOff = 0.0;

            adsr.noteOn();
        }
    }

    void stopNote(float /*velocity*/, bool allowTailOff) override
    {
        adsr.noteOff();
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                        int startSample, int numSamples) override
    {
        if (mainOscillator != nullptr)
        {
            // Process each sample
            while (--numSamples >= 0)
            {
                // Get main oscillator sample
                auto mainSample = mainOscillator->getNextSample();

                // Add subharmonic oscillators
                float subharmonicSum = 0.0f;
                for (int i = 0; i < subharmonicOscillators.size(); ++i)
                {
                    float divisor = (float)(i + 2);
                    float amplitude = 0.5f / divisor;
                    subharmonicSum += subharmonicOscillators[i]->getNextSample() * amplitude;
                }

                // Combine main + subharmonics
                auto waveformSample = mainSample + subharmonicSum;

                // Apply ADSR envelope
                auto envelopeValue = adsr.getNextSample();

                // Final sample: waveform × ADSR × velocity × anti-click
                auto currentSample = waveformSample * envelopeValue * level;

                // Write to all output channels
                for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                    outputBuffer.addSample(i, startSample, currentSample);

                ++startSample;

                // Check if envelope has finished
                if (!adsr.isActive())
                {
                    clearCurrentNote();
                    mainOscillator.reset();
                    subharmonicOscillators.clear();
                    break;
                }
            }
        }
    }

    void setADSRParameters(const juce::ADSR::Parameters& params)
    {
        adsr.setParameters(params);
    }

    void setNumSubharmonics(int num)
    {
        numSubharmonics = juce::jlimit(0, 8, num);
    }

    void setCurrentPlaybackSampleRate(double newRate) override
    {
        SynthesiserVoice::setCurrentPlaybackSampleRate(newRate);
        adsr.setSampleRate(newRate);
    }

private:
    std::unique_ptr<WavetableOscillator> mainOscillator;
    std::vector<std::unique_ptr<WavetableOscillator>> subharmonicOscillators;

    int numSubharmonics = 0;
    double level = 0.0;
    double tailOff = 0.0;
    juce::ADSR adsr;

    // NEW: Anti-click fade-in envelope (independent of ADSR)
    int antiClickSamplesRemaining = 0;
    int antiClickSamplesTotal = 0;
};