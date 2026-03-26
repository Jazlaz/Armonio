#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "WavetableOscillator.h"

//==============================================================================
class WavetableSound : public juce::SynthesiserSound
{
public:
    WavetableSound(const juce::AudioSampleBuffer& wavetableToUse)
        : wavetable(wavetableToUse) {}

    bool appliesToNote(int) override    { return true; }
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
        params.attack  = 0.01f;
        params.decay   = 0.1f;
        params.sustain = 0.8f;
        params.release = 0.3f;
        adsr.setParameters(params);
    }

    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<WavetableSound*>(sound) != nullptr;
    }

    void setLFODepth(float d)      { lfoDepth        = d; }
    void setLFOSpeed(float s)      { lfoSpeed        = s; }
    void setLFOEnabled(bool e)     { lfoEnabled      = e; }
    void setLFOMode(int m)         { lfoMode         = m; }
    void setHammondFifth(float v)          { hammondFifthLevel          = juce::jlimit(0.0f, 1.0f, v); }
    void setHammondSubOctave(float v)      { hammondSubOctaveLevel      = juce::jlimit(0.0f, 1.0f, v); }
    void setHammondOctaveUp(float v)       { hammondOctaveUpLevel       = juce::jlimit(0.0f, 1.0f, v); }
    void setHammondThirdHarmonic(float v)  { hammondThirdHarmonicLevel  = juce::jlimit(0.0f, 1.0f, v); }
    void setHammondTwoOctavesUp(float v)   { hammondTwoOctavesUpLevel   = juce::jlimit(0.0f, 1.0f, v); }
    void setHammondFifthHarmonic(float v)  { hammondFifthHarmonicLevel  = juce::jlimit(0.0f, 1.0f, v); }
    void setHammondSixthHarmonic(float v)  { hammondSixthHarmonicLevel  = juce::jlimit(0.0f, 1.0f, v); }
    void setHammondThreeOctavesUp(float v) { hammondThreeOctavesUpLevel = juce::jlimit(0.0f, 1.0f, v); }
    void setChime2Enabled(bool e)          { chime2Enabled = e; }
    void setChime3Enabled(bool e)          { chime3Enabled = e; }

    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound* sound, int) override
    {
        auto* wavetableSound = dynamic_cast<WavetableSound*>(sound);
        if (wavetableSound == nullptr)
            return;

        currentWavetable = &wavetableSound->getWavetable();
        currentMidiNote  = midiNoteNumber;
        fundamentalFreq  = (float)juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        lfoPhase         = 0.0f;

        const float sampleRate  = (float)getSampleRate();
        const float nyquistFreq = sampleRate / 2.0f;

        // Harmonic oscillators (h=1..16).
        for (int h = 1; h <= 16; ++h)
        {
            float freq = fundamentalFreq * (float)h;
            if (freq < nyquistFreq)
            {
                harmonicOscillators[h - 1].setWavetable(*currentWavetable);
                harmonicOscillators[h - 1].setFrequency(freq, sampleRate);
                harmonicOscillators[h - 1].setRandomPhase();
            }
            else
            {
                harmonicOscillators[h - 1].deactivate();
            }
        }

        // Fifth oscillator (quint) at 1.5x fundamental
        {
            float fifthFreq = fundamentalFreq * 1.5f;
            if (fifthFreq < nyquistFreq)
            {
                fifthOscillator.setWavetable(*currentWavetable);
                fifthOscillator.setFrequency(fifthFreq, sampleRate);
                fifthOscillator.setRandomPhase();
            }
            else { fifthOscillator.deactivate(); }
        }

        // Sub-octave oscillator at 0.5x fundamental (one octave lower)
        {
            float subFreq = fundamentalFreq * 0.5f;
            if (subFreq > 0.0f)
            {
                subOctaveOscillator.setWavetable(*currentWavetable);
                subOctaveOscillator.setFrequency(subFreq, sampleRate);
                subOctaveOscillator.setRandomPhase();
            }
            else { subOctaveOscillator.deactivate(); }
        }

        // Register oscillators: fundamental, +1 oct, +2 oct, +3 oct
        auto startRegister = [&](WavetableOscillator& osc, float freq)
        {
            if (freq > 0.0f && freq < nyquistFreq)
            {
                osc.setWavetable(*currentWavetable);
                osc.setFrequency(freq, sampleRate);
                osc.setRandomPhase();
            }
            else { osc.deactivate(); }
        };
        startRegister(octaveUpOscillator,        fundamentalFreq * 2.0f);
        startRegister(thirdHarmonicOscillator,   fundamentalFreq * 3.0f);
        startRegister(twoOctavesUpOscillator,    fundamentalFreq * 4.0f);
        startRegister(fifthHarmonicOscillator,   fundamentalFreq * 5.0f);
        startRegister(sixthHarmonicOscillator,   fundamentalFreq * 6.0f);
        startRegister(threeOctavesUpOscillator,  fundamentalFreq * 8.0f);

        // Chime oscillators — percussive, own ADSR, triggered once at note start
        const juce::ADSR::Parameters chimeParams { 0.005f, 0.1f, 0.0f, 0.01f };

        auto startChime = [&](WavetableOscillator& osc,
                               juce::ADSR& env, float freq, bool enabled)
        {
            if (enabled && freq < nyquistFreq)
            {
                osc.setWavetable(*currentWavetable);
                osc.setFrequency(freq, sampleRate);
                osc.setRandomPhase();
                env.setSampleRate(sampleRate);
                env.setParameters(chimeParams);
                env.reset();
                env.noteOn();
            }
            else { osc.deactivate(); }
        };

        startChime(chime2Oscillator, chime2ADSR, fundamentalFreq * 2.0f, chime2Enabled);
        startChime(chime3Oscillator, chime3ADSR, fundamentalFreq * 3.0f, chime3Enabled);

        // Subharmonic oscillators
        for (int i = 0; i < 8; ++i)
        {
            float subFreq = fundamentalFreq / (float)(i + 2);
            if (subFreq < nyquistFreq)
            {
                subharmonicOscillators[i].setWavetable(*currentWavetable);
                subharmonicOscillators[i].setFrequency(subFreq, sampleRate);
                subharmonicOscillators[i].setRandomPhase();
            }
            else
            {
                subharmonicOscillators[i].deactivate();
            }
        }

        level = velocity * 0.07f;
        fadeInCounter = fadeInLength;   // ~1.5 ms anti-click ramp

        // If this voice was stolen, fade out its last output while fading in
        if (wasStolen)
        {
            stealOffset      = lastOutputSample;
            stealFadeCounter = fadeInLength;
            wasStolen        = false;
        }
        else
        {
            stealFadeCounter = 0;
        }

        adsr.reset();
        adsr.noteOn();
    }

    void stopNote(float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            adsr.noteOff();
            chime2ADSR.noteOff();
            chime3ADSR.noteOff();
        }
        else
        {
            // Voice is being stolen — flag for crossfade in startNote
            wasStolen = true;
            clearCurrentNote();
            adsr.reset();
            chime2ADSR.reset();
            chime3ADSR.reset();
        }
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer,
                         int startSample, int numSamples) override
    {
        if (currentWavetable == nullptr)
            return;

        // --- Precompute per-block amplitudes -----------------------------------
        float ampSum = 0.0f;
        for (int h = 1; h <= 16; ++h)
            ampSum += std::abs(getHarmonicAmplitude(h)) * getBlend(h);
        float normFactor = (ampSum > 0.0f) ? 1.0f / ampSum : 1.0f;

        std::array<float, 16> hAmps{};
        int activeHarmonics = 0;
        for (int h = 1; h <= 16; ++h)
        {
            if (!harmonicOscillators[h - 1].isActive()) break;
            hAmps[h - 1] = getHarmonicAmplitude(h) * getBlend(h) * normFactor;
            if (hAmps[h - 1] != 0.0f) activeHarmonics = h;
        }

        std::array<float, 8> subAmps{};
        int activeSubs = 0;
        for (int i = 0; i < 8; ++i)
        {
            if (!subharmonicOscillators[i].isActive()) break;
            float blend = juce::jlimit(0.0f, 1.0f, numSubharmonics - (float)i);
            subAmps[i] = (0.5f / (float)(i + 2)) * blend;
            if (subAmps[i] > 0.0f) activeSubs = i + 1;
        }

        const float sr        = (float)getSampleRate();
        const bool  lfoActive = (lfoEnabled && lfoDepth > 0.00001f && activeHarmonics > 1);
        const float subSumMax   = (numSubharmonics > 0.0f)
                                  ? juce::jlimit(0.0f, 1.0f, numSubharmonics / 8.0f) * 0.91f
                                  : 0.0f;
        // Chimes are excluded from hammondNorm — they are percussive and brief,
        // so including them would permanently drop the main oscillator volume.
        const float hammondNorm = 1.0f / (1.0f + subSumMax
            + hammondFifthLevel + hammondSubOctaveLevel
            + hammondOctaveUpLevel + hammondThirdHarmonicLevel
            + hammondTwoOctavesUpLevel + hammondFifthHarmonicLevel + hammondSixthHarmonicLevel
            + hammondThreeOctavesUpLevel);
        // Chimes get a fixed scale so they blend without affecting main level
        static constexpr float chimeScale = 0.4f;
        const float twoPi     = juce::MathConstants<float>::twoPi;

        // LFO is updated every 8 samples — inaudible at these speeds, ~8x cheaper
        static constexpr int lfoInterval = 8;
        int   lfoCounter = 0;
        float lfoVal     = 0.0f;

        // --- Sample loop -------------------------------------------------------
        while (--numSamples >= 0)
        {
            // Sub-block LFO update
            if (lfoActive && --lfoCounter <= 0)
            {
                lfoCounter = lfoInterval;
                lfoPhase  += lfoSpeed * (float)lfoInterval / sr;
                if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
                lfoVal = std::sin(lfoPhase * twoPi) * lfoDepth;

                for (int h = 1; h <= activeHarmonics; ++h)
                {
                    if (!harmonicOscillators[h - 1].isActive()) continue;
                    int  pos = activeHarmonics - h + 1;
                    bool mod;
                    switch (lfoMode)
                    {
                        case 1:  mod = (h > 1 && pos % 2 == 0); break;
                        case 2:  mod = (h > 1 && pos % 3 == 0); break;
                        case 3:  mod = (h > 1 && pos % 4 == 0); break;
                        case 4:  mod = (h == 1);                 break;
                        default: mod = (h >= 2);                 break;
                    }
                    if (mod)
                        harmonicOscillators[h - 1].setFrequency(
                            fundamentalFreq * (float)h * (1.0f + lfoVal), sr);
                }
            }

            float mainSample = 0.0f;

            for (int h = 1; h <= activeHarmonics; ++h)
                mainSample += harmonicOscillators[h - 1].getNextSample() * hAmps[h - 1];

            float subSum = 0.0f;
            for (int i = 0; i < activeSubs; ++i)
                subSum += subharmonicOscillators[i].getNextSample() * subAmps[i];

            float fifthSample = (fifthOscillator.isActive() && hammondFifthLevel > 0.0001f)
                                 ? fifthOscillator.getNextSample() * hammondFifthLevel
                                 : 0.0f;

            float subOctaveSample = (subOctaveOscillator.isActive() && hammondSubOctaveLevel > 0.0001f)
                                     ? subOctaveOscillator.getNextSample() * hammondSubOctaveLevel
                                     : 0.0f;

            float octaveUpSample       = (octaveUpOscillator.isActive()       && hammondOctaveUpLevel       > 0.0001f) ? octaveUpOscillator.getNextSample()       * hammondOctaveUpLevel       : 0.0f;
            float thirdHarmonicSample  = (thirdHarmonicOscillator.isActive()  && hammondThirdHarmonicLevel  > 0.0001f) ? thirdHarmonicOscillator.getNextSample()  * hammondThirdHarmonicLevel  : 0.0f;
            float twoOctavesUpSample   = (twoOctavesUpOscillator.isActive()   && hammondTwoOctavesUpLevel   > 0.0001f) ? twoOctavesUpOscillator.getNextSample()   * hammondTwoOctavesUpLevel   : 0.0f;
            float fifthHarmonicSample  = (fifthHarmonicOscillator.isActive()  && hammondFifthHarmonicLevel  > 0.0001f) ? fifthHarmonicOscillator.getNextSample()  * hammondFifthHarmonicLevel  : 0.0f;
            float sixthHarmonicSample  = (sixthHarmonicOscillator.isActive()  && hammondSixthHarmonicLevel  > 0.0001f) ? sixthHarmonicOscillator.getNextSample()  * hammondSixthHarmonicLevel  : 0.0f;
            float threeOctavesUpSample = (threeOctavesUpOscillator.isActive() && hammondThreeOctavesUpLevel > 0.0001f) ? threeOctavesUpOscillator.getNextSample() * hammondThreeOctavesUpLevel : 0.0f;

            // Chimes have their own envelope — not gated by the main ADSR
            float chimeSample = 0.0f;
            if (chime2Oscillator.isActive())
                chimeSample += chime2Oscillator.getNextSample() * chime2ADSR.getNextSample();
            if (chime3Oscillator.isActive())
                chimeSample += chime3Oscillator.getNextSample() * chime3ADSR.getNextSample();

            auto envelopeValue = adsr.getNextSample();

            // Anti-click fade-in when a voice is stolen and restarted
            float fadeGain = 1.0f;
            if (fadeInCounter > 0)
            {
                fadeGain = 1.0f - (float)fadeInCounter / (float)fadeInLength;
                --fadeInCounter;
            }

            float stealFade = 0.0f;
            if (stealFadeCounter > 0)
            {
                stealFade = (float)stealFadeCounter / (float)fadeInLength;
                --stealFadeCounter;
            }

            auto currentSample = ((mainSample + subSum + fifthSample + subOctaveSample
                                + octaveUpSample + thirdHarmonicSample
                                + twoOctavesUpSample + fifthHarmonicSample + sixthHarmonicSample
                                + threeOctavesUpSample) * hammondNorm * envelopeValue
                                + chimeSample * chimeScale) * (float)level * fadeGain
                                + stealOffset * stealFade;

            lastOutputSample = currentSample;

            for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                outputBuffer.addSample(i, startSample, currentSample);

            ++startSample;

            if (!adsr.isActive()
                && !chime2ADSR.isActive()
                && !chime3ADSR.isActive())
            {
                clearCurrentNote();
                deactivateOscillators();
                break;
            }
        }
    }

    // ------------------------------------------------------------------
    void setWaveform(int waveformType) { currentWaveform = waveformType; }
    void setNumHarmonics(float num)    { numHarmonics    = num; }
    void setNumSubharmonics(float num) { numSubharmonics = juce::jlimit(0.0f, 8.0f, num); }

    void setADSRParameters(const juce::ADSR::Parameters& params)
    {
        adsr.setParameters(params);
    }

    void setCurrentPlaybackSampleRate(double newRate) override
    {
        SynthesiserVoice::setCurrentPlaybackSampleRate(newRate);
        if (newRate > 0.0)
        {
            adsr.setSampleRate(newRate);
            chime2ADSR.setSampleRate(newRate);
            chime3ADSR.setSampleRate(newRate);
        }
    }

private:
    float getBlend(int h) const
    {
        if (currentWaveform == 1 || currentWaveform == 2)
        {
            if (h % 2 == 0) return 0.0f;
            int step = (h + 1) / 2;
            return juce::jlimit(0.0f, 1.0f, numHarmonics - (float)(step - 1));
        }
        return juce::jlimit(0.0f, 1.0f, numHarmonics - (float)(h - 1));
    }

    float getHarmonicAmplitude(int h) const
    {
        switch (currentWaveform)
        {
            case 0: return 1.0f / (float)h;

            case 1:
                if (h % 2 == 0) return 0.0f;
                return (1.0f / (float)h) * (4.0f / juce::MathConstants<float>::pi);

            case 2:
                if (h % 2 == 0) return 0.0f;
                {
                    int   step = (h + 1) / 2;
                    float sign = (step % 2 == 0) ? -1.0f : 1.0f;
                    return sign / (float)(h * h)
                           * (8.0f / (juce::MathConstants<float>::pi * juce::MathConstants<float>::pi));
                }
            default: return 0.0f;
        }
    }

    void deactivateOscillators()
    {
        for (auto& osc : harmonicOscillators)    osc.deactivate();
        for (auto& osc : subharmonicOscillators) osc.deactivate();
        fifthOscillator.deactivate();
        subOctaveOscillator.deactivate();
        octaveUpOscillator.deactivate();
        thirdHarmonicOscillator.deactivate();
        twoOctavesUpOscillator.deactivate();
        fifthHarmonicOscillator.deactivate();
        sixthHarmonicOscillator.deactivate();
        threeOctavesUpOscillator.deactivate();
        chime2Oscillator.deactivate();
        chime3Oscillator.deactivate();
        currentWavetable = nullptr;
    }

    // -------------------------------------------------------------------------
    // Pre-allocated oscillator pools — no heap allocation on the audio thread
    std::array<WavetableOscillator, 16> harmonicOscillators;
    std::array<WavetableOscillator,  8> subharmonicOscillators;
    WavetableOscillator                  fifthOscillator;
    WavetableOscillator                  subOctaveOscillator;
    WavetableOscillator                  octaveUpOscillator;
    WavetableOscillator                  thirdHarmonicOscillator;
    WavetableOscillator                  twoOctavesUpOscillator;
    WavetableOscillator                  fifthHarmonicOscillator;
    WavetableOscillator                  sixthHarmonicOscillator;
    WavetableOscillator                  threeOctavesUpOscillator;
    WavetableOscillator                  chime2Oscillator;
    WavetableOscillator                  chime3Oscillator;
    juce::ADSR                           chime2ADSR, chime3ADSR;

    const juce::AudioSampleBuffer* currentWavetable = nullptr;
    int    currentMidiNote  = 69;
    int    currentWaveform  = 0;
    float  numHarmonics     = 1.0f;
    float  numSubharmonics  = 0.0f;
    float  fundamentalFreq  = 0.0f;
    double level            = 0.0;
    juce::ADSR adsr;

    static constexpr int fadeInLength = 64;  // ~1.5 ms at 44100 Hz
    int   fadeInCounter     = 0;
    float lastOutputSample  = 0.0f;
    bool  wasStolen         = false;
    float stealOffset       = 0.0f;
    int   stealFadeCounter  = 0;

    float lfoPhase          = 0.0f;
    float lfoDepth          = 0.0f;
    float lfoSpeed          = 0.5f;
    bool  lfoEnabled        = false;
    int   lfoMode           = 0;
    float hammondFifthLevel          = 0.0f;
    float hammondSubOctaveLevel      = 0.0f;
    float hammondOctaveUpLevel       = 0.0f;
    float hammondThirdHarmonicLevel  = 0.0f;
    float hammondTwoOctavesUpLevel   = 0.0f;
    float hammondFifthHarmonicLevel  = 0.0f;
    float hammondSixthHarmonicLevel  = 0.0f;
    float hammondThreeOctavesUpLevel = 0.0f;
    bool  chime2Enabled              = false;
    bool  chime3Enabled         = false;
};
