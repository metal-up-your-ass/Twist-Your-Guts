#include "PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

// Issue #58: getTailLengthSeconds() unconditionally returned 0.0, correct
// only while IRLoader has its single-sample identity IR installed (the
// safe-by-default state before any cab IR is loaded). loadImpulseResponse()
// is already a live, callable seam into a real juce::dsp::Convolution with
// no length cap, so once a real IR is loaded the plugin's actual output tail
// grows to that IR's length while getTailLengthSeconds() kept reporting 0.0
// - a host trusting that value for bounce/freeze/render-tail decisions could
// truncate the convolution tail.
namespace
{
    // A short synthetic "IR" of a known, precise duration, so the assertion
    // below can check getTailLengthSeconds() against an exact expected value
    // rather than just ">0".
    juce::AudioBuffer<float> makeImpulseResponseOfLength (int numIrSamples)
    {
        juce::AudioBuffer<float> ir (2, numIrSamples);

        for (int channel = 0; channel < ir.getNumChannels(); ++channel)
        {
            auto* data = ir.getWritePointer (channel);

            for (int sample = 0; sample < numIrSamples; ++sample)
                data[sample] = std::exp (-static_cast<float> (sample) / 8.0f);
        }

        return ir;
    }
}

TEST_CASE ("getTailLengthSeconds(): reports 0 with no IR loaded (safe-by-default identity IR)", "[tail][ir]")
{
    CryptaAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    CHECK (processor.getTailLengthSeconds() == Catch::Approx (0.0).margin (1.0e-9));
}

TEST_CASE ("getTailLengthSeconds(): reflects the loaded IR's actual length, not a hardcoded 0", "[tail][ir]")
{
    CryptaAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    constexpr int irSampleRate = 48000;
    constexpr int numIrSamples = 4800; // exactly 0.1s at 48kHz

    processor.loadImpulseResponse (makeImpulseResponseOfLength (numIrSamples), static_cast<double> (irSampleRate));

    const auto expectedTailSeconds = static_cast<double> (numIrSamples) / static_cast<double> (irSampleRate);

    CHECK (processor.getTailLengthSeconds() > 0.0);
    CHECK (processor.getTailLengthSeconds() == Catch::Approx (expectedTailSeconds).margin (1.0e-6));
}

TEST_CASE ("getTailLengthSeconds(): tracks the IR's own duration independent of the session sample rate", "[tail][ir]")
{
    CryptaAudioProcessor processor;
    // Session runs at 96kHz; the IR itself was captured/generated at 44.1kHz
    // - the reported tail must reflect the IR's real-world duration, not get
    // conflated with the session's sample rate.
    processor.prepareToPlay (96000.0, 512);

    constexpr int irSampleRate = 44100;
    constexpr int numIrSamples = 2205; // exactly 0.05s at 44.1kHz

    processor.loadImpulseResponse (makeImpulseResponseOfLength (numIrSamples), static_cast<double> (irSampleRate));

    const auto expectedTailSeconds = static_cast<double> (numIrSamples) / static_cast<double> (irSampleRate);

    CHECK (processor.getTailLengthSeconds() == Catch::Approx (expectedTailSeconds).margin (1.0e-6));
}
