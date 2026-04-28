#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "WavetableSound.h"
#include "WaveformGenerator.h"

// Synthesiser subclass that customises default JUCE voice-management for this
// synth's use case (long releases + only 12 primary voices):
//   1. noteOn(): if the same pitch is already releasing, reuse that voice
//      instead of starting a second voice on the same note.
//   2. noteOn(): before stealing a voice, try to hand its release tail off to
//      a free voice from the 4-voice fade pool so the tail keeps playing to
//      its natural end while the primary restarts cleanly.
//   3. findVoiceToSteal(): prefer whichever non-held voice is already quietest
//      — cutting a near-silent tail is inaudible, cutting a loud one clicks.
//      Fade-pool voices are skipped by stealing; they're managed by this class.
// All voices added to this synth must be WavetableVoice (cast is unchecked).
class HammondSynth : public juce::Synthesiser
{
public:
    void noteOn(int midiChannel, int midiNoteNumber, float velocity) override
    {
        const juce::ScopedLock sl(lock);

        // --- Find a free voice OR steal one. If stealing, hand the tail off
        // to the fade pool so it keeps decaying naturally. ---
        for (int s = 0; s < getNumSounds(); ++s)
        {
            auto sound = getSound(s);
            if (! sound->appliesToNote(midiNoteNumber))    continue;
            if (! sound->appliesToChannel(midiChannel))    continue;

            auto* freeVoice = findFreePrimary(sound.get());
            if (freeVoice != nullptr)
            {
                startVoice(freeVoice, sound.get(), midiChannel, midiNoteNumber, velocity);
                return;
            }

            // No free primary — before stealing from a healthy voice, check if
            // any primary is already in a quick-fade (on its way to silence).
            // Kill it immediately to reclaim the slot; it was doomed anyway and
            // is already near-silent, so there's no audible discontinuity.
            auto* fadingVoice = findQuickFadingPrimary(sound.get());
            if (fadingVoice != nullptr)
            {
                fadingVoice->forceKill();
                // The voice is now inactive — findFreePrimary will return it.
                auto* recycled = findFreePrimary(sound.get());
                if (recycled != nullptr)
                {
                    startVoice(recycled, sound.get(), midiChannel, midiNoteNumber, velocity);
                    return;
                }
            }

            // Still no free slot — fall back to normal voice stealing
            auto* victim = findVoiceToSteal(sound.get(), midiChannel, midiNoteNumber);
            if (victim != nullptr)
            {
                auto* wv = static_cast<WavetableVoice*>(victim);
                tryHandoffToFadePool(wv, sound.get());
                startVoice(victim, sound.get(), midiChannel, midiNoteNumber, velocity);
                return;
            }
        }
    }

    juce::SynthesiserVoice* findVoiceToSteal(juce::SynthesiserSound* sound,
                                              int midiChannel,
                                              int midiNoteNumber) const override
    {
        juce::SynthesiserVoice* best = nullptr;
        float lowestEnv = 2.0f;  // ADSR output is 0..1, so 2 is a safe sentinel

        for (int i = 0; i < getNumVoices(); ++i)
        {
            auto* v = getVoice(i);
            if (! v->canPlaySound(sound)) continue;
            if (! v->isVoiceActive())     continue;

            auto* wv = static_cast<WavetableVoice*>(v);
            if (wv->isFadeVoice())  continue;  // fade pool is off-limits to stealing
            if (wv->isNoteHeld())   continue;  // never steal a held note

            const float env = wv->getLastEnvelopeValue();
            if (env < lowestEnv)
            {
                lowestEnv = env;
                best      = v;
            }
        }

        if (best != nullptr) return best;

        return juce::Synthesiser::findVoiceToSteal(sound, midiChannel, midiNoteNumber);
    }

private:
    // Walks only primary voices (skips the fade pool) and returns the first
    // inactive one that can play the sound.
    juce::SynthesiserVoice* findFreePrimary(juce::SynthesiserSound* sound) const
    {
        for (int i = 0; i < getNumVoices(); ++i)
        {
            auto* v = getVoice(i);
            auto* wv = static_cast<WavetableVoice*>(v);
            if (wv->isFadeVoice())         continue;
            if (! v->canPlaySound(sound))  continue;
            if (v->isVoiceActive())        continue;
            return v;
        }
        return nullptr;
    }

    // Returns the first primary voice that is currently in a quick-fade.
    // These are already doomed — safe to kill instantly to reclaim the slot.
    WavetableVoice* findQuickFadingPrimary(juce::SynthesiserSound* sound) const
    {
        for (int i = 0; i < getNumVoices(); ++i)
        {
            auto* v  = getVoice(i);
            auto* wv = static_cast<WavetableVoice*>(v);
            if (wv->isFadeVoice())        continue;
            if (! v->canPlaySound(sound)) continue;
            if (! v->isVoiceActive())     continue;
            if (wv->isQuickFading())      return wv;
        }
        return nullptr;
    }

    // Find an inactive fade-pool voice, copy the dying primary's state into
    // it, and let it play out the natural release tail. No-op if no fade
    // voice is free — primary then falls back to its own scalar crossfade.
    void tryHandoffToFadePool(WavetableVoice* dying, juce::SynthesiserSound* sound)
    {
        for (int i = 0; i < getNumVoices(); ++i)
        {
            auto* v  = getVoice(i);
            auto* wv = static_cast<WavetableVoice*>(v);
            if (! wv->isFadeVoice())   continue;
            if (v->isVoiceActive())    continue;

            // Arm the fade voice so its next startNote will takeOverFrom(dying).
            // Channel/note/velocity passed to startVoice are placeholders — the
            // real state comes from the source. Channel = 1 is a safe default
            // (JUCE's startVoice only uses it for the base class bookkeeping).
            wv->setPendingFadeSource(dying);
            startVoice(v, sound, 1, dying->getCurrentlyPlayingNote(), 0.0f);
            return;
        }
    }
};

class SynthAudioSource : public juce::AudioSource
{
public:
    explicit SynthAudioSource(juce::MidiKeyboardState& keyState);

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill,
                           juce::MidiBuffer& midiMessages);

    void setWaveform(int waveformType);
    void setADSRParameters(float attack, float decay, float sustain, float release);
    void setNumHarmonics(float numHarmonics);
    void setNumSubharmonics(float numSubharmonics);
    void setLFODepth(float depth);
    void setLFOSpeed(float speed);
    void setLFOEnabled(bool enabled);
    void setLFOMode(int mode);
    void setHammondFifth(float level);
    void setHammondSubOctave(float level);
    void setHammondOctaveUp(float level);
    void setHammondThirdHarmonic(float level);
    void setHammondTwoOctavesUp(float level);
    void setHammondFifthHarmonic(float level);
    void setHammondSixthHarmonic(float level);
    void setHammondThreeOctavesUp(float level);
    void setChime2Enabled(bool enabled);
    void setChime3Enabled(bool enabled);
    // Returns the number of primary voices currently active (held + releasing).
    // Fade-pool voices are excluded — they are implementation detail, not musical notes.
    int getActiveVoiceCount() const noexcept
    {
        int count = 0;
        for (int i = 0; i < kNumPrimaryVoices; ++i)
            if (voices[i] != nullptr && voices[i]->isVoiceActive())
                ++count;
        return count;
    }

    void setWindLevel(float level);
    void setWindThreshold(float freqHz);
    void setWindUpperThreshold(float freqHz);
    void setWindWidth(int n);
    void setChiffLevel(float level);
    void setJIEnabled(bool enabled);
    void setJIRootKey(int key);

private:
    // Called every block. If more than 10 primary voices are active, finds the
    // released voice that has been releasing the longest and starts a 60 ms
    // linear fade-out on it so the slot is freed without a click.
    void trimExcessVoices() noexcept
    {
        static constexpr int kVoiceLimit = 8;

        int activeCount = 0;
        for (int i = 0; i < kNumPrimaryVoices; ++i)
            if (voices[i] != nullptr && voices[i]->isVoiceActive())
                ++activeCount;

        if (activeCount > kVoiceLimit)
        {
            // Over the limit: fade out the longest-releasing tail.
            WavetableVoice* oldest = nullptr;
            int maxAge = -1;
            for (int i = 0; i < kNumPrimaryVoices; ++i)
            {
                auto* v = voices[i];
                if (v == nullptr || !v->isVoiceActive()) continue;
                if (v->isNoteHeld())    continue;   // never cut a held note
                if (v->isQuickFading()) continue;   // already being faded out
                if (v->getReleaseAge() > maxAge)
                {
                    maxAge = v->getReleaseAge();
                    oldest = v;
                }
            }
            if (oldest != nullptr)
                oldest->triggerQuickFade(currentSampleRate);
        }
        else
        {
            // Back under the limit: cancel any quick-fades still in progress
            // so those voices finish their natural release instead.
            for (int i = 0; i < kNumPrimaryVoices; ++i)
            {
                auto* v = voices[i];
                if (v != nullptr && v->isQuickFading())
                    v->cancelQuickFade();
            }
        }
    }
    // 12 primary voices play incoming notes. 4 fade voices exist solely to
    // take over the release tail of a primary that's being stolen, so the
    // tail can decay naturally instead of being scalar-faded in ~50 ms.
    static constexpr int kNumPrimaryVoices = 16;
    static constexpr int kNumFadeVoices    = 2;
    static constexpr int kNumVoices        = kNumPrimaryVoices + kNumFadeVoices;

    juce::MidiKeyboardState& keyboardState;
    HammondSynth synth;

    // Cached typed pointers — populated once in the constructor so setters don't
    // need dynamic_cast on every call (RTTI is ~60 cycles each on Cortex-A53).
    std::array<WavetableVoice*, kNumVoices> voices {};

    juce::AudioSampleBuffer sineWavetable;  // single pure sine, shared by all oscillators

    int    currentWaveform        = 0;
    float  currentNumHarmonics    = 1.0f;
    float  currentNumSubharmonics = 0.0f;
    double currentSampleRate      = 44100.0;
    float  currentLFODepth        = 0.0f;
    float  currentLFOSpeed        = 0.5f;
    bool   currentLFOEnabled      = false;
    int    currentLFOMode         = 0;
    float  currentHammondFifth           = 0.0f;
    float  currentHammondSubOctave       = 0.0f;
    float  currentHammondOctaveUp        = 0.0f;
    float  currentHammondThirdHarmonic   = 0.0f;
    float  currentHammondTwoOctavesUp    = 0.0f;
    float  currentHammondFifthHarmonic   = 0.0f;
    float  currentHammondSixthHarmonic   = 0.0f;
    float  currentHammondThreeOctavesUp  = 0.0f;
    bool   currentChime2Enabled          = false;
    bool   currentChime3Enabled          = false;
    float  currentWindLevel              = 0.0f;
    float  currentWindThreshold          = 1000.0f;
    float  currentWindUpperThreshold     = 20000.0f;
    int    currentWindWidth              = 8;
    float  currentChiffLevel             = 0.5f;
    bool   currentJIEnabled              = false;
    int    currentJIRootKey              = 0;

    void initializeWavetables();
    void pushStateToVoices();
};
