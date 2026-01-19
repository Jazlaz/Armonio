#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

class WaveformGenerator
{
public:
    static juce::AudioSampleBuffer createSineWave(unsigned int tableSize,
                                                   int numHarmonics,
                                                   float fundamentalFreq = 440.0f,
                                                   float sampleRate = 44100.0f)
    {
        juce::AudioSampleBuffer sineTable;
        sineTable.setSize(1, (int)tableSize + 1);
        sineTable.clear();

        auto* samples = sineTable.getWritePointer(0);
        float nyquistFreq = sampleRate / 2.0f;

        // Add harmonics
        for (int harmonic = 1; harmonic <= numHarmonics; ++harmonic)
        {
            // Band-limiting
            float harmonicFreq = fundamentalFreq * harmonic;
            if (harmonicFreq >= nyquistFreq)
                break;

            float amplitude = 1.0f / (float)harmonic;

            auto angleDelta = (juce::MathConstants<double>::twoPi / (double)(tableSize - 1)) * harmonic;
            auto currentAngle = 0.0;

            for (unsigned int i = 0; i < tableSize; ++i)
            {
                samples[i] += (float)std::sin(currentAngle) * amplitude;
                currentAngle += angleDelta;
            }
        }

        normalizeWaveform(samples, tableSize);
        samples[tableSize] = samples[0];
        return sineTable;
    }

    static juce::AudioSampleBuffer createSawWave(unsigned int tableSize,
                                                  int numHarmonics,
                                                  float fundamentalFreq = 440.0f,
                                                  float sampleRate = 44100.0f)
    {
        juce::AudioSampleBuffer sawTable;
        sawTable.setSize(1, (int)tableSize + 1);
        sawTable.clear();

        auto* samples = sawTable.getWritePointer(0);

        float nyquistFreq = sampleRate / 2.0f;

        // Add harmonics
        for (int harmonic = 1; harmonic <= numHarmonics; ++harmonic)
        {
            float harmonicFreq = fundamentalFreq * harmonic;
            if (harmonicFreq >= nyquistFreq)
                break;

            float amplitude = 1.0f / harmonic;
            float detune = 1.0f + (harmonic * 0.05f);

            auto angleDelta = (juce::MathConstants<double>::twoPi / (double)(tableSize - 1));
            auto currentAngle = 0.0;

            for (unsigned int i = 0; i < tableSize; ++i)
            {
                samples[i] += std::sin(currentAngle * harmonic * detune) * amplitude;
                currentAngle += angleDelta;
            }
        }

        normalizeWaveform(samples, tableSize);
        samples[tableSize] = samples[0];
        return sawTable;
    }

    static juce::AudioSampleBuffer createSquareWave(unsigned int tableSize,
                                                     int numHarmonics,
                                                     float fundamentalFreq = 440.0f,
                                                     float sampleRate = 44100.0f)
    {
        juce::AudioSampleBuffer squareTable;
        squareTable.setSize(1, (int)tableSize + 1);
        squareTable.clear();

        auto* samples = squareTable.getWritePointer(0);

        float nyquistFreq = sampleRate / 2.0f;

        // Add harmonics
        for (int n = 1; n <= numHarmonics; ++n)
        {
            int harmonic = 2 * n - 1;  // 1, 3, 5, 7...

            float harmonicFreq = fundamentalFreq * harmonic;
            if (harmonicFreq >= nyquistFreq)
                break;

            float amplitude = 1.0f / (float)harmonic;

            auto angleDelta = (juce::MathConstants<double>::twoPi / (double)(tableSize - 1)) * harmonic;
            auto currentAngle = 0.0;

            for (unsigned int i = 0; i < tableSize; ++i)
            {
                samples[i] += (float)std::sin(currentAngle) * amplitude * (4.0f / juce::MathConstants<float>::pi);
                currentAngle += angleDelta;
            }
        }

        normalizeWaveform(samples, tableSize);
        samples[tableSize] = samples[0];
        return squareTable;
    }

    static juce::AudioSampleBuffer createTriangleWave(unsigned int tableSize,
                                                       int numHarmonics,
                                                       float fundamentalFreq = 440.0f,
                                                       float sampleRate = 44100.0f)
    {
        juce::AudioSampleBuffer triangleTable;
        triangleTable.setSize(1, (int)tableSize + 1);
        triangleTable.clear();

        auto* samples = triangleTable.getWritePointer(0);

        float nyquistFreq = sampleRate / 2.0f;

        // Add harmonics
        for (int n = 1; n <= numHarmonics; ++n)
        {
            int harmonic = 2 * n - 1;

            float harmonicFreq = fundamentalFreq * harmonic;
            if (harmonicFreq >= nyquistFreq)
                break;

            float amplitude = 1.0f / (float)(harmonic * harmonic);
            const float sign = (n % 2 == 0) ? -1.0f : 1.0f;

            auto angleDelta = (juce::MathConstants<double>::twoPi / (double)(tableSize - 1)) * harmonic;
            auto currentAngle = 0.0;

            for (unsigned int i = 0; i < tableSize; ++i)
            {
                samples[i] += (float)std::sin(currentAngle) * amplitude * sign * (8.0f / (juce::MathConstants<float>::pi * juce::MathConstants<float>::pi));
                currentAngle += angleDelta;
            }
        }

        normalizeWaveform(samples, tableSize);
        samples[tableSize] = samples[0];
        return triangleTable;
    }

private:
    // Normalize waveform
    static void normalizeWaveform(float* samples, const unsigned int tableSize)
    {
        float maxVal = 0.0f;
        for (unsigned int i = 0; i < tableSize; ++i)
        {
            float absVal = std::abs(samples[i]);
            if (absVal > maxVal)
                maxVal = absVal;
        }

        if (maxVal > 0.0f)
        {
            const float scale = 1.0f / maxVal;
            for (unsigned int i = 0; i < tableSize; ++i)
            {
                samples[i] *= scale;
            }
        }
    }
};