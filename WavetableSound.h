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
    void setHammondFifth(float v)          { hammondFifthSmooth         .setTargetValue(juce::jlimit(0.0f, 1.0f, v)); }
    void setHammondSubOctave(float v)      { hammondSubOctaveSmooth     .setTargetValue(juce::jlimit(0.0f, 1.0f, v)); }
    void setHammondOctaveUp(float v)       { hammondOctaveUpSmooth      .setTargetValue(juce::jlimit(0.0f, 1.0f, v)); }
    void setHammondThirdHarmonic(float v)  { hammondThirdHarmonicSmooth .setTargetValue(juce::jlimit(0.0f, 1.0f, v)); }
    void setHammondTwoOctavesUp(float v)   { hammondTwoOctavesUpSmooth  .setTargetValue(juce::jlimit(0.0f, 1.0f, v)); }
    void setHammondFifthHarmonic(float v)  { hammondFifthHarmonicSmooth .setTargetValue(juce::jlimit(0.0f, 1.0f, v)); }
    void setHammondSixthHarmonic(float v)  { hammondSixthHarmonicSmooth .setTargetValue(juce::jlimit(0.0f, 1.0f, v)); }
    void setHammondThreeOctavesUp(float v) { hammondThreeOctavesUpSmooth.setTargetValue(juce::jlimit(0.0f, 1.0f, v)); }
    void setChime2Enabled(bool e)          { chime2Enabled = e; }
    void setChime3Enabled(bool e)          { chime3Enabled = e; }

    void startNote(int midiNoteNumber, float velocity,
                   juce::SynthesiserSound* sound, int) override
    {
        // Fade-pool handoff path: when HammondSynth routes a dying primary's
        // tail to us, it sets pendingFadeSource + calls startVoice. Copy state
        // from the source and return — we play its natural release, nothing
        // else. The midi note / velocity passed in are just placeholders so
        // the base class marks us active.
        if (fadeVoice && pendingFadeSource != nullptr)
        {
            takeOverFrom(*pendingFadeSource);
            pendingFadeSource->markHandoffHandled();
            pendingFadeSource = nullptr;
            return;
        }

        auto* wavetableSound = dynamic_cast<WavetableSound*>(sound);
        if (wavetableSound == nullptr)
            return;

        currentWavetable = &wavetableSound->getWavetable();
        currentMidiNote  = midiNoteNumber;
        fundamentalFreq  = computeJIFrequency(midiNoteNumber);
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

        // Chime oscillators — percussive, own ADSR, triggered once at note start.
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

        // Air harmonic oscillators — equal-amplitude harmonics above windThreshold
        activeAirOscillators = 0;
        airWobblePhase = airRng.nextFloat(); // random start so voices breathe independently
        chiffEnv = 0.0f;
        chiffAttacking = true;
        {
            int startH = (int)std::ceil(windThreshold / fundamentalFreq);
            startH = juce::jmax(1, startH);
            for (int i = 0; i < numAirHarmonics; ++i)
            {
                float freq = fundamentalFreq * (float)(startH + i);
                // Slight inharmonicity: real pipe harmonics stretch quadratically with partial number
                freq *= 1.0f + 0.0003f * (float)(i * i);
                // Random detune ±3 cents per harmonic — breaks up mechanical regularity
                freq *= std::pow(2.0f, (airRng.nextFloat() * 6.0f - 3.0f) / 1200.0f);
                if (freq < nyquistFreq * 0.95f && freq < windUpperThreshold)
                {
                    airOscillators[i].setWavetable(*currentWavetable);
                    airOscillators[i].setFrequency(freq, sampleRate);
                    airOscillators[i].setRandomPhase();
                    ++activeAirOscillators;
                }
                else
                {
                    airOscillators[i].deactivate();
                }
            }
        }

        level = velocity * 0.3f;
        fadeInCounter = fadeInLength;   // ~5 ms anti-click ramp

        // If this voice was stolen AND the fade pool didn't already take the
        // tail, fade out its last output while fading in. When a fade voice
        // grabbed the tail we skip this — otherwise the tail would be heard
        // twice (once from the fade voice, once from our scalar offset).
        if (wasStolen && !handoffHandled)
        {
            stealOffset      = lastOutputSample;
            stealFadeCounter = stealFadeLength;  // ~50 ms
        }
        else
        {
            stealFadeCounter = 0;
        }
        wasStolen      = false;
        handoffHandled = false;

        inRelease      = false;
        releaseEnv     = 0.0f;
        adsr.reset();
        adsr.noteOn();
        noteIsHeld = true;
    }

    void stopNote(float, bool allowTailOff) override
    {
        noteIsHeld = false;
        if (allowTailOff)
        {
            // Start our exponential release from wherever the envelope currently
            // is — this handles releases triggered mid-attack or mid-decay cleanly.
            inRelease       = true;
            releaseEnv      = lastEnvelopeValue;
            releaseAge      = 0;
            quickFadeActive = false;
            quickFadeGain   = 1.0f;
            // Still tell juce::ADSR to release so chime ADSRs stay in sync;
            // we won't use the main ADSR's output once inRelease is true.
            adsr.noteOff();
            chime2ADSR.noteOff();
            chime3ADSR.noteOff();
        }
        else
        {
            // Voice is being stolen — flag for crossfade in startNote
            inRelease  = false;
            releaseEnv = 0.0f;
            wasStolen  = true;
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

        // Track how long this voice has been releasing so SynthAudioSource can
        // find the oldest tail and fade it out when voice count exceeds 10.
        if (!noteIsHeld)
            releaseAge += numSamples;

        // --- Re-init air oscillators if threshold/width changed while note is held ---
        if (airOscDirty.exchange(false) && fundamentalFreq > 0.0f)
        {
            const float sr          = (float)getSampleRate();
            const float nyquist     = sr * 0.5f;
            activeAirOscillators    = 0;
            int startH = (int)std::ceil(windThreshold / fundamentalFreq);
            startH = juce::jmax(1, startH);
            for (int i = 0; i < numAirHarmonics; ++i)
            {
                float freq = fundamentalFreq * (float)(startH + i);
                freq *= 1.0f + 0.0003f * (float)(i * i);
                if (freq < nyquist * 0.95f && freq < windUpperThreshold)
                {
                    airOscillators[i].setWavetable(*currentWavetable);
                    airOscillators[i].setFrequency(freq, sr);
                    // keep existing phase — avoids a click on frequency change
                    ++activeAirOscillators;
                }
                else
                {
                    airOscillators[i].deactivate();
                }
            }
        }

        // --- Advance harmonics / subharmonics smoothers by one full block ------
        // getBlend() reads numHarmonics / numSubharmonics directly, so we write
        // the smoothed value back before the precompute and the sample loop sees
        // a gradually changing blend each block — no per-sample overhead.
        numHarmonics    = numHarmonicsSmooth   .skip(numSamples);
        numSubharmonics = numSubharmonicsSmooth.skip(numSamples);

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

        // Build a compact list of only the active Hammond drawbars. This moves
        // 16 per-sample branches (isActive + level>0.0001f, ×8) into a single
        // per-block walk, and turns the sample-loop body into a flat multiply-add
        // the compiler can keep in registers / vectorise with NEON.
        struct HammondEntry { WavetableOscillator* osc; juce::SmoothedValue<float>* smoother; };
        std::array<HammondEntry, 8> activeHammond;
        int numActiveHammond = 0;
        auto addIfActive = [&](WavetableOscillator& o, juce::SmoothedValue<float>& sm) {
            // Include if audible now OR still ramping toward a target
            if (o.isActive() && (sm.getCurrentValue() > 0.0001f || sm.isSmoothing()))
                activeHammond[numActiveHammond++] = { &o, &sm };
        };
        addIfActive(fifthOscillator,          hammondFifthSmooth);
        addIfActive(subOctaveOscillator,      hammondSubOctaveSmooth);
        addIfActive(octaveUpOscillator,       hammondOctaveUpSmooth);
        addIfActive(thirdHarmonicOscillator,  hammondThirdHarmonicSmooth);
        addIfActive(twoOctavesUpOscillator,   hammondTwoOctavesUpSmooth);
        addIfActive(fifthHarmonicOscillator,  hammondFifthHarmonicSmooth);
        addIfActive(sixthHarmonicOscillator,  hammondSixthHarmonicSmooth);
        addIfActive(threeOctavesUpOscillator, hammondThreeOctavesUpSmooth);

        const float sr        = (float)getSampleRate();
        const bool  lfoActive = (lfoEnabled && lfoDepth > 0.00001f && activeHarmonics > 1);
        // Fixed normalization: denominator is always the theoretical maximum
        // (1 fundamental + 8 drawbars at full + subharmonics at full = 9.91).
        // Drawbars are now purely additive — pulling a new one never reduces
        // the level of already-active oscillators. Pure sine is quieter;
        // adding drawbars makes it progressively louder, like a real Hammond.
        // Chimes are still excluded (independent percussive envelope).
        static constexpr float hammondNorm = 1.0f / (1.0f + 8.0f + 0.91f);
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

            float hammondSum = 0.0f;
            for (int k = 0; k < numActiveHammond; ++k)
                hammondSum += activeHammond[k].osc->getNextSample() * activeHammond[k].smoother->getNextValue();

            // Chimes have their own envelope — not gated by the main ADSR
            float chimeSample = 0.0f;
            if (chime2Oscillator.isActive())
                chimeSample += chime2Oscillator.getNextSample() * chime2ADSR.getNextSample();
            if (chime3Oscillator.isActive())
                chimeSample += chime3Oscillator.getNextSample() * chime3ADSR.getNextSample();

            // Exponential release: multiply by coeff each sample (one-pole decay).
            // Attack and decay still use juce::ADSR's linear ramps.
            float envelopeValue;
            if (inRelease)
            {
                releaseEnv *= releaseCoeff;
                envelopeValue = releaseEnv;
            }
            else
            {
                envelopeValue = adsr.getNextSample();
            }
            lastEnvelopeValue = envelopeValue;

            // Anti-click fade-in when a voice is stolen and restarted.
            // Smoothstep (t²·(3−2t)) instead of linear: zero derivative at both
            // endpoints removes the kink that a linear ramp leaves in the
            // waveform — that kink is what you hear as a "click" on steal.
            float fadeGain = 1.0f;
            if (fadeInCounter > 0)
            {
                const float t = 1.0f - (float)fadeInCounter / (float)fadeInLength;
                fadeGain = t * t * (3.0f - 2.0f * t);
                --fadeInCounter;
            }

            float stealFade = 0.0f;
            if (stealFadeCounter > 0)
            {
                const float t = (float)stealFadeCounter / (float)stealFadeLength;
                stealFade = t * t * (3.0f - 2.0f * t);
                --stealFadeCounter;
            }

            // Air harmonics — correlated high-frequency harmonics above windThreshold
            float airContrib = 0.0f;
            if (activeAirOscillators > 0)
            {
                float rawAir = 0.0f, weightSum = 0.0f;
                for (int i = 0; i < activeAirOscillators; ++i)
                {
                    const float w = 1.0f / (float)(i + 1); // 1/f rolloff
                    rawAir += airOscillators[i].getNextSample() * w;
                    weightSum += w;
                }
                rawAir /= weightSum;

                // Slow wobble (0.5 Hz) — bellows pressure variation
                airWobblePhase += airWobbleIncrement;
                if (airWobblePhase >= 1.0f) airWobblePhase -= 1.0f;
                rawAir *= 1.0f + 0.2f * std::sin(airWobblePhase * juce::MathConstants<float>::twoPi);

                // Chiff AD envelope: fast attack then slow decay, independent of note ADSR
                if (chiffAttacking)
                {
                    chiffEnv += chiffAttackCoeff * (1.0f - chiffEnv);
                    if (chiffEnv > 0.99f) chiffAttacking = false;
                }
                else
                {
                    chiffEnv += chiffDecayCoeff * (0.0f - chiffEnv);
                }

                // Steady wind follows note ADSR; chiff burst bypasses it
                airContrib = rawAir * windLevel * (envelopeValue * 0.06f + chiffEnv * chiffLevel * 0.3f);
            }

            auto currentSample = ((mainSample + subSum + hammondSum + chimeSample * chimeScale) * hammondNorm * envelopeValue)
                                * (float)level * fadeGain
                                + airContrib
                                + stealOffset * stealFade;

            // Smart cull: if SynthAudioSource flagged this voice for a quick
            // 60 ms fade-out, apply and decrement the gain every sample.
            if (quickFadeActive)
            {
                currentSample *= quickFadeGain;
                quickFadeGain -= quickFadeStep;
            }

            lastOutputSample = currentSample;

            for (auto i = outputBuffer.getNumChannels(); --i >= 0;)
                outputBuffer.addSample(i, startSample, currentSample);

            ++startSample;

            // Voice is done when:
            //   a) the exponential release tail has decayed to inaudibility, OR
            //   b) a quick-fade has finished (gain reached zero), OR
            //   c) envelope dropped below -50 dB while released
            // In all cases, both chime envelopes must also be finished.
            const bool releaseDone  = inRelease ? (releaseEnv < 0.0001f) : !adsr.isActive();
            const bool quickFadeDone = quickFadeActive && (quickFadeGain <= 0.0f);
            const bool belowFloor   = !noteIsHeld && (envelopeValue < 0.01f);
            if ((releaseDone || quickFadeDone || belowFloor)
                && !chime2ADSR.isActive()
                && !chime3ADSR.isActive())
            {
                inRelease       = false;
                releaseEnv      = 0.0f;
                quickFadeActive = false;
                quickFadeGain   = 1.0f;
                clearCurrentNote();
                deactivateOscillators();
                break;
            }
        }
    }

    // ------------------------------------------------------------------
    void setWaveform(int waveformType) { currentWaveform = waveformType; }
    void setNumHarmonics(float num)    { numHarmonicsSmooth   .setTargetValue(num); }
    void setNumSubharmonics(float num) { numSubharmonicsSmooth.setTargetValue(juce::jlimit(0.0f, 8.0f, num)); }

    void setADSRParameters(const juce::ADSR::Parameters& params)
    {
        currentRelease = params.release;
        adsr.setParameters(params);
        recomputeReleaseCoeff();
    }

    void setCurrentPlaybackSampleRate(double newRate) override
    {
        SynthesiserVoice::setCurrentPlaybackSampleRate(newRate);
        if (newRate > 0.0)
        {
            adsr.setSampleRate(newRate);
            chime2ADSR.setSampleRate(newRate);
            chime3ADSR.setSampleRate(newRate);

            // Smoothers — 8ms ramp, preserves current value so no glitch on prepare
            const double ramp = 0.008;
            hammondFifthSmooth         .reset(newRate, ramp);
            hammondSubOctaveSmooth     .reset(newRate, ramp);
            hammondOctaveUpSmooth      .reset(newRate, ramp);
            hammondThirdHarmonicSmooth .reset(newRate, ramp);
            hammondTwoOctavesUpSmooth  .reset(newRate, ramp);
            hammondFifthHarmonicSmooth .reset(newRate, ramp);
            hammondSixthHarmonicSmooth .reset(newRate, ramp);
            hammondThreeOctavesUpSmooth.reset(newRate, ramp);
            numHarmonicsSmooth         .reset(newRate, ramp);
            numSubharmonicsSmooth      .reset(newRate, ramp);

            airWobbleIncrement = 0.5f / (float)newRate;
            chiffAttackCoeff   = 1.0f - std::exp(-1.0f / (0.015f * (float)newRate)); // 15ms
            chiffDecayCoeff    = 1.0f - std::exp(-1.0f / (0.4f   * (float)newRate)); // 400ms
            // ~5 ms crossfade at any sample rate; long enough to hide steal clicks
            // without noticeably delaying the new note's attack.
            fadeInLength       = juce::jmax(32, (int)(0.005 * newRate));
            // ~50 ms scalar tail fade when a voice is stolen and no fade-pool
            // voice is free. Much longer than the fade-in because the ear is far
            // more sensitive to a release tail abruptly ending than to a new
            // attack ramping up.
            stealFadeLength    = juce::jmax(128, (int)(0.05 * newRate));
            recomputeReleaseCoeff();
        }
    }

    // Used by HammondSynth for smarter voice stealing: prefer voices that are
    // already releasing and already quiet.
    bool  isNoteHeld()           const noexcept { return noteIsHeld; }
    float getLastEnvelopeValue() const noexcept { return lastEnvelopeValue; }
    int   getReleaseAge()        const noexcept { return releaseAge; }
    bool  isQuickFading()        const noexcept { return quickFadeActive; }

    // Arm a 200 ms linear fade-to-zero so this releasing voice frees its slot
    // without a click. Called by SynthAudioSource when voice count exceeds 8.
    void triggerQuickFade(double sampleRate) noexcept
    {
        if (quickFadeActive) return;          // already fading — don't restart
        quickFadeActive = true;
        quickFadeGain   = 1.0f;
        quickFadeStep   = 1.0f / (0.01f * (float)sampleRate);
    }

    // Instantly kill this voice so its primary slot can be reclaimed.
    // Only called on quick-fading voices — they are already near-silent so
    // there is no audible discontinuity. Mirrors the render-loop kill path.
    void forceKill() noexcept
    {
        inRelease       = false;
        releaseEnv      = 0.0f;
        quickFadeActive = false;
        quickFadeGain   = 1.0f;
        clearCurrentNote();
        deactivateOscillators();
    }

    // Cancel an in-progress quick-fade when the voice count drops back under 8.
    // Folds the current attenuation into releaseEnv so the tail continues from
    // its already-reduced level — no discontinuity, no click.
    void cancelQuickFade() noexcept
    {
        if (!quickFadeActive) return;
        releaseEnv     *= quickFadeGain;   // absorb attenuation into the release tail
        quickFadeActive = false;
        quickFadeGain   = 1.0f;
    }

    // Fade-pool plumbing. Four dedicated voices are flagged via setFadeVoice.
    // When a primary is about to be stolen, HammondSynth copies its state into
    // a free fade voice (via setPendingFadeSource + startVoice), and the fade
    // voice plays out the natural release tail. markHandoffHandled tells the
    // primary's next startNote to skip its own scalar crossfade — otherwise
    // the two fades would sum.
    void setFadeVoice(bool v) noexcept           { fadeVoice = v; }
    bool isFadeVoice() const noexcept            { return fadeVoice; }
    void setPendingFadeSource(WavetableVoice* s) { pendingFadeSource = s; }
    void markHandoffHandled() noexcept           { handoffHandled = true; }

    // Copy audio-producing state from a dying primary voice. Called from the
    // fade voice's startNote when pendingFadeSource is set. Uses the dying
    // voice's own release parameters so the tail decays at the exact rate the
    // user set — no forced fast release.
    void takeOverFrom(const WavetableVoice& src)
    {
        currentMidiNote   = src.currentMidiNote;
        fundamentalFreq   = src.fundamentalFreq;
        level             = src.level;
        currentWavetable  = src.currentWavetable;
        currentWaveform   = src.currentWaveform;
        numHarmonics      = src.numHarmonics;
        numSubharmonics   = src.numSubharmonics;

        harmonicOscillators       = src.harmonicOscillators;
        subharmonicOscillators    = src.subharmonicOscillators;
        fifthOscillator           = src.fifthOscillator;
        subOctaveOscillator       = src.subOctaveOscillator;
        octaveUpOscillator        = src.octaveUpOscillator;
        thirdHarmonicOscillator   = src.thirdHarmonicOscillator;
        twoOctavesUpOscillator    = src.twoOctavesUpOscillator;
        fifthHarmonicOscillator   = src.fifthHarmonicOscillator;
        sixthHarmonicOscillator   = src.sixthHarmonicOscillator;
        threeOctavesUpOscillator  = src.threeOctavesUpOscillator;
        chime2Oscillator          = src.chime2Oscillator;
        chime3Oscillator          = src.chime3Oscillator;
        for (int i = 0; i < maxAirHarmonics; ++i)
            airOscillators[i] = src.airOscillators[i];
        activeAirOscillators = src.activeAirOscillators;

        adsr       = src.adsr;
        chime2ADSR = src.chime2ADSR;
        chime3ADSR = src.chime3ADSR;

        // Copy smoothed Hammond levels — snap to current value so the fade voice
        // starts at the same level as the stolen primary with no ramp artifact.
        hammondFifthSmooth         .setCurrentAndTargetValue(src.hammondFifthSmooth         .getCurrentValue());
        hammondSubOctaveSmooth     .setCurrentAndTargetValue(src.hammondSubOctaveSmooth     .getCurrentValue());
        hammondOctaveUpSmooth      .setCurrentAndTargetValue(src.hammondOctaveUpSmooth      .getCurrentValue());
        hammondThirdHarmonicSmooth .setCurrentAndTargetValue(src.hammondThirdHarmonicSmooth .getCurrentValue());
        hammondTwoOctavesUpSmooth  .setCurrentAndTargetValue(src.hammondTwoOctavesUpSmooth  .getCurrentValue());
        hammondFifthHarmonicSmooth .setCurrentAndTargetValue(src.hammondFifthHarmonicSmooth .getCurrentValue());
        hammondSixthHarmonicSmooth .setCurrentAndTargetValue(src.hammondSixthHarmonicSmooth .getCurrentValue());
        hammondThreeOctavesUpSmooth.setCurrentAndTargetValue(src.hammondThreeOctavesUpSmooth.getCurrentValue());
        chime2Enabled              = src.chime2Enabled;
        chime3Enabled              = src.chime3Enabled;

        lfoPhase   = src.lfoPhase;
        lfoDepth   = src.lfoDepth;
        lfoSpeed   = src.lfoSpeed;
        lfoEnabled = src.lfoEnabled;
        lfoMode    = src.lfoMode;

        windLevel          = src.windLevel;
        windThreshold      = src.windThreshold;
        windUpperThreshold = src.windUpperThreshold;
        numAirHarmonics    = src.numAirHarmonics;
        airWobblePhase     = src.airWobblePhase;
        airWobbleIncrement = src.airWobbleIncrement;
        chiffEnv           = src.chiffEnv;
        chiffAttacking     = src.chiffAttacking;
        chiffAttackCoeff   = src.chiffAttackCoeff;
        chiffDecayCoeff    = src.chiffDecayCoeff;
        chiffLevel         = src.chiffLevel;

        lastOutputSample  = src.lastOutputSample;
        lastEnvelopeValue = src.lastEnvelopeValue;

        // Fresh fade-state — we're now the one doing the release
        fadeInCounter    = 0;
        wasStolen        = false;
        stealFadeCounter = 0;
        stealOffset      = 0.0f;
        noteIsHeld       = false;

        // Push all envelopes into release mode (no-op if already in release).
        // Natural release rate — sounds identical to the dying voice's tail.
        adsr.noteOff();
        chime2ADSR.noteOff();
        chime3ADSR.noteOff();
    }


    void setWindLevel(float v)           { windLevel          = juce::jlimit(0.0f, 1.0f, v); }
    void setWindThreshold(float freqHz)  { windThreshold      = juce::jlimit(500.0f, 10000.0f, freqHz); airOscDirty.store(true); }
    void setWindUpperThreshold(float freqHz) { windUpperThreshold = juce::jlimit(1000.0f, 20000.0f, freqHz); airOscDirty.store(true); }
    void setWindWidth(int n)             { numAirHarmonics    = juce::jlimit(1, maxAirHarmonics, n); airOscDirty.store(true); }
    void setChiffLevel(float v)          { chiffLevel         = juce::jlimit(0.0f, 1.0f, v); }

    void setJIEnabled(bool e) { jiEnabled = e; }
    void setJIRootKey(int k)  { jiRootKey = juce::jlimit(0, 11, k); }

private:
    // Returns the fundamental frequency, applying JI ratios when enabled.
    float computeJIFrequency(int midiNote) const
    {
        if (!jiEnabled)
            return (float)juce::MidiMessage::getMidiNoteInHertz(midiNote);

        // 5-limit just intonation (Ptolemy's intense diatonic)
        static constexpr float jiRatios[12] = {
            1.0f,          // unison       1/1
            16.0f/15.0f,   // minor 2nd    16/15
            9.0f/8.0f,     // major 2nd    9/8
            6.0f/5.0f,     // minor 3rd    6/5
            5.0f/4.0f,     // major 3rd    5/4
            4.0f/3.0f,     // perfect 4th  4/3
            45.0f/32.0f,   // tritone      45/32
            3.0f/2.0f,     // perfect 5th  3/2
            8.0f/5.0f,     // minor 6th    8/5
            5.0f/3.0f,     // major 6th    5/3
            9.0f/5.0f,     // minor 7th    9/5
            15.0f/8.0f     // major 7th    15/8
        };
        int semiFromRoot = ((midiNote - jiRootKey) % 12 + 12) % 12;
        int rootMidi     = midiNote - semiFromRoot;
        return (float)juce::MidiMessage::getMidiNoteInHertz(rootMidi) * jiRatios[semiFromRoot];
    }
    void recomputeReleaseCoeff()
    {
        const double sr = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
        // One-pole exponential decay: env *= coeff each sample.
        // coeff = exp(-1 / (T * sr)) gives a -60 dB decay in time T.
        releaseCoeff = (float)std::exp(-1.0 / (juce::jmax(0.001f, currentRelease) * sr));
    }

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

    // Fade length is set in setCurrentPlaybackSampleRate to ~5 ms regardless of
    // sample rate. Default covers 44.1 kHz until prepareToPlay runs.
    int   fadeInLength      = 220;   // ~5 ms at 44100 Hz
    int   fadeInCounter     = 0;
    float lastOutputSample  = 0.0f;
    bool  wasStolen         = false;
    float stealOffset       = 0.0f;
    int   stealFadeCounter  = 0;
    // Separate, longer fade used ONLY for the scalar-tail crossfade when a voice
    // is stolen without a fade-pool handoff. ~50 ms at any sample rate.
    int   stealFadeLength   = 2205;  // ~50 ms at 44100 Hz
    bool  noteIsHeld        = false;  // true from startNote until stopNote(tailOff=true)
    float lastEnvelopeValue = 0.0f;   // cached per-sample; read by HammondSynth

    // Exponential release state. Once stopNote(tailOff=true) fires, inRelease
    // takes over from juce::ADSR and multiplies releaseEnv by releaseCoeff
    // every sample — a true one-pole exponential decay, like an analog envelope.
    bool  inRelease         = false;
    float releaseEnv        = 0.0f;
    float releaseCoeff      = 0.9999f;  // recomputed in setADSRParameters / setCurrentPlaybackSampleRate
    float currentRelease    = 0.3f;     // release time in seconds, mirrors last setADSRParameters call

    // Smart voice-cull system: tracks how long a voice has been releasing
    // (in samples) so SynthAudioSource can find the oldest tail to fade out.
    int   releaseAge        = 0;
    bool  quickFadeActive   = false;
    float quickFadeGain     = 1.0f;
    float quickFadeStep     = 0.0f;  // per-sample decrement; set by triggerQuickFade()

    // Fade-pool state. fadeVoice==true means this WavetableVoice belongs to the
    // pool of 4 release-only voices. pendingFadeSource is set by HammondSynth
    // just before startVoice() so the next startNote can copy state from the
    // dying primary. handoffHandled tells the primary's *own* next startNote
    // that its dying tail was already taken over, so it must NOT run its
    // scalar-tail crossfade (we'd hear the tail twice otherwise).
    bool             fadeVoice           = false;
    bool             handoffHandled      = false;
    WavetableVoice*  pendingFadeSource   = nullptr;

    float lfoPhase          = 0.0f;
    float lfoDepth          = 0.0f;
    float lfoSpeed          = 0.5f;
    bool  lfoEnabled        = false;
    int   lfoMode           = 0;
    // Hammond drawbar levels — SmoothedValue gives click-free knob moves (8ms ramp)
    juce::SmoothedValue<float> hammondFifthSmooth;
    juce::SmoothedValue<float> hammondSubOctaveSmooth;
    juce::SmoothedValue<float> hammondOctaveUpSmooth;
    juce::SmoothedValue<float> hammondThirdHarmonicSmooth;
    juce::SmoothedValue<float> hammondTwoOctavesUpSmooth;
    juce::SmoothedValue<float> hammondFifthHarmonicSmooth;
    juce::SmoothedValue<float> hammondSixthHarmonicSmooth;
    juce::SmoothedValue<float> hammondThreeOctavesUpSmooth;
    // Harmonics / subharmonics smoothers — block-level, written into numHarmonics /
    // numSubharmonics before each precompute so getBlend() sees the ramp.
    juce::SmoothedValue<float> numHarmonicsSmooth    { 1.0f };
    juce::SmoothedValue<float> numSubharmonicsSmooth { 0.0f };
    bool  chime2Enabled = false;
    bool  chime3Enabled = false;

    float windLevel          = 0.0f;
    float windThreshold      = 1000.0f;  // lowest frequency for air harmonics (Hz)
    float windUpperThreshold = 20000.0f; // highest frequency for air harmonics (Hz)
    std::atomic<bool> airOscDirty { false };  // set by setters, cleared in renderNextBlock

    static constexpr int maxAirHarmonics = 32;
    int  numAirHarmonics     = 8;   // settable width (1–32)
    WavetableOscillator airOscillators[maxAirHarmonics];
    int activeAirOscillators = 0;

    juce::Random airRng;
    float airWobblePhase     = 0.0f;
    float airWobbleIncrement = 0.5f / 44100.0f;  // 0.5 Hz, corrected in setCurrentPlaybackSampleRate
    float chiffEnv           = 0.0f;              // 0→1 attack, 1→0 decay envelope
    bool  chiffAttacking     = false;
    float chiffAttackCoeff   = 0.00151f;          // ~15ms at 44100, corrected in setCurrentPlaybackSampleRate
    float chiffDecayCoeff    = 0.0000567f;        // ~400ms at 44100, corrected in setCurrentPlaybackSampleRate
    float chiffLevel         = 0.5f;              // controlled by slider

    bool jiEnabled = false;
    int  jiRootKey = 0;   // 0=C, 1=C#, ..., 11=B
};
