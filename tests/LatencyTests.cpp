#include "PluginProcessor.h"
#include "params/ParameterIds.h"
#include "TestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

// Issue #9: latency-compensation framework acceptance gates. As of M1 no
// oversampling exists yet, so the seam (TwistYourGutsAudioProcessor::
// computeTotalLatencySamples()/updateLatencyCompensation(), called from
// prepareToPlay()) always computes/reports zero - these tests pin that down
// and also confirm the seam doesn't itself break the band-split-then-sum
// magnitude flatness that issue #8 establishes at the Crossover-class level.
namespace
{
    constexpr int testBlockSize = 512;
}

TEST_CASE ("Latency: plugin reports zero latency at multiple sample rates/block sizes", "[latency][dsp]")
{
    const double sampleRates[] = { 44100.0, 48000.0, 96000.0 };
    const int blockSizes[] = { 64, 256, 512, 1024 };

    for (const auto sampleRate : sampleRates)
    {
        for (const auto blockSize : blockSizes)
        {
            TwistYourGutsAudioProcessor processor;
            processor.prepareToPlay (sampleRate, blockSize);

            INFO ("sampleRate = " << sampleRate << ", blockSize = " << blockSize);
            CHECK (processor.getLatencySamples() == 0);
        }
    }
}

TEST_CASE ("Latency: re-preparing the processor keeps latency at zero", "[latency][dsp]")
{
    TwistYourGutsAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    CHECK (processor.getLatencySamples() == 0);

    // Simulates a host changing sample rate/block size mid-session (e.g. a
    // DAW sample-rate switch), which re-runs the latency-compensation seam.
    processor.prepareToPlay (96000.0, 256);
    CHECK (processor.getLatencySamples() == 0);
}

TEST_CASE ("Latency: band-split-then-sum preserves magnitude flatness through the full processor", "[latency][dsp][crossover]")
{
    // Exercises the same flat-sum property as CrossoverTests.cpp, but end-
    // to-end through TwistYourGutsAudioProcessor::processBlock() - i.e.
    // including the zero-latency compensation delay line on the low band -
    // to confirm the #9 seam doesn't perturb the #8 flat-sum guarantee.
    constexpr double testSampleRate = 48000.0;

    const double probeFrequenciesHz[] = { 60.0, 150.0, 250.0, 600.0, 2000.0, 8000.0 };

    for (const auto probeFrequencyHz : probeFrequenciesHz)
    {
        TwistYourGutsAudioProcessor processor;
        processor.prepareToPlay (testSampleRate, testBlockSize);

        juce::AudioBuffer<float> buffer (2, testBlockSize);
        juce::MidiBuffer midi;

        // Settle gain smoothing and the crossover's filter transient.
        for (int i = 0; i < 12; ++i)
        {
            TestHelpers::fillWithSine (buffer, testSampleRate, probeFrequencyHz);
            processor.processBlock (buffer, midi);
        }

        juce::AudioBuffer<float> reference (2, testBlockSize);
        TestHelpers::fillWithSine (reference, testSampleRate, probeFrequencyHz);

        const auto inputRms = TestHelpers::rms (reference);
        const auto outputRms = TestHelpers::rms (buffer);

        REQUIRE (inputRms > 0.0);

        INFO ("probe frequency = " << probeFrequencyHz << " Hz");
        CHECK (juce::Decibels::gainToDecibels (outputRms / inputRms) == Catch::Approx (0.0).margin (0.1));
    }
}
