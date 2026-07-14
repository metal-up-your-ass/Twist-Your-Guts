#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/Crossover.h"

// As of M1 (issues #7-#10), the APVTS layout declares the complete, ID-frozen
// v1.0 parameter set (see src/params/ParameterIds.h and ParameterLayout.h),
// and processBlock() implements the gain-staged LR4 crossover core: input
// trim -> crossover split -> per-band level -> sum -> optional safety clip ->
// output trim. Compression, high-band voicing/drive, EQ and the IR loader
// remain declared-but-inert until their own milestone (M2 dynamics, M3
// distortion, M4 EQ/IR) wires them into the signal chain.
class TwistYourGutsAudioProcessor final : public juce::AudioProcessor
{
public:
    TwistYourGutsAudioProcessor();
    ~TwistYourGutsAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorParameter* getBypassParameter() const override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;

private:
    //==============================================================================
    // Latency-compensation seam (issue #9): computes the plugin's total
    // reported latency and re-arms the low-band compensation delay to match
    // it. Currently always 0 (no oversampling exists yet), but this is a
    // real seam, not a mislabeled no-op - M3's oversampled high-band voicing
    // stage will report a non-zero latency here, and the low-band delay
    // line below will pick it up automatically so both bands stay time-
    // aligned when summed. Called once from prepareToPlay().
    int computeTotalLatencySamples() const noexcept;
    void updateLatencyCompensation();

    // Processes at most `preparedBlockSize` samples: crossover split, per-
    // band level, sum, optional safety clip. Called once per chunk from
    // processBlock() so oversized host blocks (larger than prepareToPlay
    // promised) are handled defensively without ever resizing a buffer on
    // the audio thread.
    void processChunk (juce::dsp::AudioBlock<float>& chunk) noexcept;

    //==============================================================================
    juce::dsp::Gain<float> inputGainProcessor;
    juce::dsp::Gain<float> outputGainProcessor;

    // Issue #8: LR4 crossover splitting the (input-trimmed) signal into low
    // and high bands ahead of independent per-band processing.
    tyg::Crossover crossover;

    // Issue #10: independent per-band level trims applied after the split
    // and before the bands are summed back together.
    juce::dsp::Gain<float> lowGainProcessor;
    juce::dsp::Gain<float> highGainProcessor;

    // Issue #9: upper bound on the latency this plugin will ever need to
    // compensate for, i.e. the largest oversampling latency M3's high-band
    // voicing stage is expected to introduce. Generous headroom (a few
    // hundred samples at typical sample rates covers even high-order/high-
    // ratio oversampling FIR latencies) at negligible memory cost.
    static constexpr int maxLatencyCompensationSamples = 1024;

    // Delays the low band so it stays time-aligned with the high band once
    // M3 gives the high band a non-zero oversampling latency. None-
    // interpolation is correct here: latency compensation is always an
    // integer number of samples, never a fractionally modulated delay.
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> lowBandLatencyDelay { maxLatencyCompensationSamples };

    // Pre-allocated band buffers, sized to `preparedBlockSize` in
    // prepareToPlay(). Never resized in processBlock().
    juce::AudioBuffer<float> lowBandBuffer;
    juce::AudioBuffer<float> highBandBuffer;
    int preparedBlockSize = 0;

    // Raw atomic pointers into the APVTS-managed parameter values, resolved
    // once at construction time so processBlock() never has to search for
    // them (no allocation/locks on the audio thread).
    std::atomic<float>* inputGainDb = nullptr;
    std::atomic<float>* outputGainDb = nullptr;
    std::atomic<float>* bypassFlag = nullptr;
    std::atomic<float>* outputClipEnabled = nullptr;
    std::atomic<float>* crossoverFreqHz = nullptr;
    std::atomic<float>* lowLevelDb = nullptr;
    std::atomic<float>* highLevelDb = nullptr;

    // The actual parameter object handed back from getBypassParameter() so
    // hosts can offer their own bypass UI/automation for this parameter.
    juce::RangedAudioParameter* bypassParameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwistYourGutsAudioProcessor)
};
