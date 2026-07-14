#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>

// Small shared helpers used across the Tests target. Kept dependency-free
// (just juce_audio_basics) so it can be included from any test file.
namespace TestHelpers
{
    // Fills every channel of the buffer with a sine wave of the given
    // frequency. `startSampleIndex` offsets the phase as if this buffer were
    // a continuation of a longer signal starting at absolute sample 0 - pass
    // 0 (the default) for phase-at-zero-on-each-call behaviour (fine for
    // memoryless gain-only passthrough checks), or the running sample count
    // when feeding a filter/IIR-stateful processor block-by-block, where a
    // phase discontinuity at every block boundary would inject spurious
    // broadband energy into the filter and pollute level measurements.
    inline void fillWithSine (juce::AudioBuffer<float>& buffer,
                              double sampleRate,
                              double frequencyHz,
                              float amplitude = 0.5f,
                              juce::int64 startSampleIndex = 0)
    {
        const auto numChannels = buffer.getNumChannels();
        const auto numSamples = buffer.getNumSamples();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* data = buffer.getWritePointer (channel);

            for (int sample = 0; sample < numSamples; ++sample)
            {
                const auto phase = juce::MathConstants<double>::twoPi * frequencyHz
                                    * static_cast<double> (startSampleIndex + sample) / sampleRate;
                data[sample] = amplitude * static_cast<float> (std::sin (phase));
            }
        }
    }

    // Root-mean-square level across all channels/samples in the buffer.
    inline double rms (const juce::AudioBuffer<float>& buffer)
    {
        double sumOfSquares = 0.0;
        juce::int64 numValues = 0;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            {
                const auto value = static_cast<double> (data[sample]);
                sumOfSquares += value * value;
                ++numValues;
            }
        }

        return numValues > 0 ? std::sqrt (sumOfSquares / static_cast<double> (numValues)) : 0.0;
    }

    // Returns true if every sample in the buffer is finite (no NaN/Inf).
    inline bool allSamplesFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* data = buffer.getReadPointer (channel);

            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                if (! std::isfinite (data[sample]))
                    return false;
        }

        return true;
    }
}
