#include "PluginProcessor.h"
#include "TestHelpers.h"

#include <catch2/catch_test_macros.hpp>

// Issue #56: CryptaAudioProcessor did not override juce::AudioProcessor::
// reset(), so a host transport-stop/loop/rewind (which calls reset(), not
// prepareToPlay()) never flushed the per-stage DSP state below. Every stage
// in src/dsp/ exposes its own reset() specifically for this purpose, but
// nothing wired them into the processor-level override before this fix -
// most audibly, the LR4 crossover's own filter memory (plus the low-band
// compensation delay line, gate/compressor envelopes, and the high-band
// voicing's oversampling/mid/tone filter state) would otherwise keep
// ringing a decaying tail into what should be dead silence after a loop
// point or stop-and-rewind.
TEST_CASE ("reset(): clears DSP stage state so silence right after a loud signal is actually silent", "[state][reset]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    CryptaAudioProcessor processor;
    processor.prepareToPlay (sampleRate, blockSize);

    juce::MidiBuffer midi;

    // Build up substantial filter/envelope/oversampling state across every
    // always-active stage (crossover, low-band compressor, high-band
    // voicing, plus the low-band latency-compensation delay line) with a
    // loud, low-frequency-rich signal run long enough to reach a settled
    // state, not just a single transient block.
    juce::AudioBuffer<float> excite (2, blockSize);

    for (int i = 0; i < 24; ++i)
    {
        TestHelpers::fillWithSine (excite, sampleRate, 80.0, 0.8f, static_cast<juce::int64> (i) * blockSize);
        processor.processBlock (excite, midi);
    }

    // Simulates a host transport stop/loop/rewind: JUCE calls reset() (not
    // prepareToPlay()) to tell the plugin to flush any tails/state left
    // running from the region that just played.
    processor.reset();

    // Silence in, right after reset(), must produce (near) silence out - any
    // measurable output here can only be unflushed state ringing on its own,
    // since there is no input to drive it.
    juce::AudioBuffer<float> silence (2, blockSize);
    silence.clear();

    processor.processBlock (silence, midi);

    CHECK (TestHelpers::allSamplesFinite (silence));
    CHECK (TestHelpers::rms (silence) < 1.0e-4);
}
