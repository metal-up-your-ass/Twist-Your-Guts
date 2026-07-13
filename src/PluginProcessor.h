#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// M0 skeleton: a clean passthrough processor with input/output gain and
// bypass. Band-split/compression/distortion DSP arrives in later milestones.
//
// As of M1 (issue #7), the APVTS layout declares the complete, ID-frozen
// v1.0 parameter set (see src/params/ParameterIds.h and ParameterLayout.h),
// but processBlock() below still only reads inputGain/outputGain/bypass -
// every other parameter is declared-but-inert until its own milestone
// (M2 dynamics, M3 distortion, M4 EQ/IR) wires it into the signal chain.
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
    juce::dsp::Gain<float> inputGainProcessor;
    juce::dsp::Gain<float> outputGainProcessor;

    // Raw atomic pointers into the APVTS-managed parameter values, resolved
    // once at construction time so processBlock() never has to search for
    // them (no allocation/locks on the audio thread).
    std::atomic<float>* inputGainDb = nullptr;
    std::atomic<float>* outputGainDb = nullptr;
    std::atomic<float>* bypassFlag = nullptr;

    // The actual parameter object handed back from getBypassParameter() so
    // hosts can offer their own bypass UI/automation for this parameter.
    juce::RangedAudioParameter* bypassParameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TwistYourGutsAudioProcessor)
};
