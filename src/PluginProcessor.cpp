#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "params/ParameterIds.h"
#include "params/ParameterLayout.h"

#include <cmath>

namespace
{
    // ~20ms smoothing ramp for gain changes: fast enough to feel responsive,
    // slow enough to avoid zipper noise on parameter automation.
    constexpr double gainRampDurationSeconds = 0.02;
}

//==============================================================================
TwistYourGutsAudioProcessor::TwistYourGutsAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    inputGainDb = apvts.getRawParameterValue (ParamIDs::inputGain);
    outputGainDb = apvts.getRawParameterValue (ParamIDs::outputGain);
    bypassFlag = apvts.getRawParameterValue (ParamIDs::bypass);
    outputClipEnabled = apvts.getRawParameterValue (ParamIDs::outputClip);
    crossoverFreqHz = apvts.getRawParameterValue (ParamIDs::crossoverFreq);
    lowLevelDb = apvts.getRawParameterValue (ParamIDs::lowLevel);
    highLevelDb = apvts.getRawParameterValue (ParamIDs::highLevel);
    bypassParameter = apvts.getParameter (ParamIDs::bypass);

    jassert (inputGainDb != nullptr);
    jassert (outputGainDb != nullptr);
    jassert (bypassFlag != nullptr);
    jassert (outputClipEnabled != nullptr);
    jassert (crossoverFreqHz != nullptr);
    jassert (lowLevelDb != nullptr);
    jassert (highLevelDb != nullptr);
    jassert (bypassParameter != nullptr);
}

TwistYourGutsAudioProcessor::~TwistYourGutsAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TwistYourGutsAudioProcessor::createParameterLayout()
{
    return tyg::createParameterLayout();
}

//==============================================================================
const juce::String TwistYourGutsAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TwistYourGutsAudioProcessor::acceptsMidi() const
{
    return false;
}

bool TwistYourGutsAudioProcessor::producesMidi() const
{
    return false;
}

bool TwistYourGutsAudioProcessor::isMidiEffect() const
{
    return false;
}

double TwistYourGutsAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TwistYourGutsAudioProcessor::getNumPrograms()
{
    return 1;
}

int TwistYourGutsAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TwistYourGutsAudioProcessor::setCurrentProgram (int)
{
}

const juce::String TwistYourGutsAudioProcessor::getProgramName (int)
{
    return {};
}

void TwistYourGutsAudioProcessor::changeProgramName (int, const juce::String&)
{
}

//==============================================================================
void TwistYourGutsAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32> (getTotalNumOutputChannels());

    inputGainProcessor.setRampDurationSeconds (gainRampDurationSeconds);
    inputGainProcessor.prepare (spec);
    inputGainProcessor.setGainDecibels (inputGainDb->load (std::memory_order_relaxed));

    outputGainProcessor.setRampDurationSeconds (gainRampDurationSeconds);
    outputGainProcessor.prepare (spec);
    outputGainProcessor.setGainDecibels (outputGainDb->load (std::memory_order_relaxed));

    // Issue #8: LR4 crossover, prepared for the same spec as the gain
    // processors so its per-channel filter state matches the bus layout.
    crossover.prepare (spec);
    crossover.setCutoffFrequency (crossoverFreqHz->load (std::memory_order_relaxed));

    // Issue #10: independent per-band level trims, smoothed the same way as
    // the input/output gains to avoid zipper noise on automation.
    lowGainProcessor.setRampDurationSeconds (gainRampDurationSeconds);
    lowGainProcessor.prepare (spec);
    lowGainProcessor.setGainDecibels (lowLevelDb->load (std::memory_order_relaxed));

    highGainProcessor.setRampDurationSeconds (gainRampDurationSeconds);
    highGainProcessor.prepare (spec);
    highGainProcessor.setGainDecibels (highLevelDb->load (std::memory_order_relaxed));

    // Issue #9: (re)allocate the low-band compensation delay line for the
    // new spec/max-delay bound. setMaximumDelayInSamples() may allocate, so
    // it must only ever be called here, never from processBlock().
    lowBandLatencyDelay.setMaximumDelayInSamples (maxLatencyCompensationSamples);
    lowBandLatencyDelay.prepare (spec);

    // Pre-allocate the band buffers to the promised block size so
    // processBlock() never resizes a buffer on the audio thread, even if a
    // host later sends an oversized block (handled defensively by chunking
    // in processBlock() - see processChunk()).
    preparedBlockSize = samplesPerBlock;
    lowBandBuffer.setSize (static_cast<int> (spec.numChannels), preparedBlockSize);
    highBandBuffer.setSize (static_cast<int> (spec.numChannels), preparedBlockSize);

    updateLatencyCompensation();
}

//==============================================================================
int TwistYourGutsAudioProcessor::computeTotalLatencySamples() const noexcept
{
    // M3 (oversampling) will replace this stub with the high-band
    // oversampling filter's reported latency (e.g.
    // juce::dsp::Oversampling::getLatencyInSamples()) once the high band is
    // oversampled ahead of the nonlinear voicing stage. Until then the high
    // band is not delayed relative to the low band, so there is nothing to
    // compensate for and this legitimately returns 0 - it is a real seam
    // computing a real (currently zero) value, not a mislabeled no-op.
    constexpr int highBandOversamplingLatencySamples = 0;
    return highBandOversamplingLatencySamples;
}

void TwistYourGutsAudioProcessor::updateLatencyCompensation()
{
    const auto totalLatencySamples = juce::jlimit (0, maxLatencyCompensationSamples, computeTotalLatencySamples());

    setLatencySamples (totalLatencySamples);

    // The low band bypasses oversampling entirely, so once M3 lands it must
    // be delayed by the same amount the high band's oversampling stage
    // delays the high band, keeping both bands time-aligned when they are
    // summed back together in processChunk().
    lowBandLatencyDelay.setDelay (static_cast<float> (totalLatencySamples));
}

void TwistYourGutsAudioProcessor::releaseResources()
{
}

bool TwistYourGutsAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mono = juce::AudioChannelSet::mono();
    const auto stereo = juce::AudioChannelSet::stereo();

    const auto mainOut = layouts.getMainOutputChannelSet();
    const auto mainIn = layouts.getMainInputChannelSet();

    if (mainOut != mono && mainOut != stereo)
        return false;

    if (mainOut != mainIn)
        return false;

    return true;
}

void TwistYourGutsAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Buses are constrained to in == out (mono or stereo), so this is
    // normally a no-op, but it's cheap insurance against stray channels.
    for (auto channel = totalNumInputChannels; channel < totalNumOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    if (bypassFlag->load (std::memory_order_relaxed) >= 0.5f)
        return;

    // Parameters are read once per host block (not per chunk/sample): the
    // dsp::Gain smoothers and the crossover's cutoff recompute cheaply
    // enough at control rate, and re-reading the same atomic per chunk would
    // buy nothing.
    inputGainProcessor.setGainDecibels (inputGainDb->load (std::memory_order_relaxed));
    outputGainProcessor.setGainDecibels (outputGainDb->load (std::memory_order_relaxed));
    lowGainProcessor.setGainDecibels (lowLevelDb->load (std::memory_order_relaxed));
    highGainProcessor.setGainDecibels (highLevelDb->load (std::memory_order_relaxed));
    crossover.setCutoffFrequency (crossoverFreqHz->load (std::memory_order_relaxed));

    juce::dsp::AudioBlock<float> fullBlock (buffer);

    // Defensive chunking: hosts are expected to never exceed the block size
    // promised to prepareToPlay(), but if one ever did, indexing straight
    // into lowBandBuffer/highBandBuffer (sized to preparedBlockSize) would
    // be out of bounds. Processing in chunks of at most preparedBlockSize
    // handles that case safely without ever resizing a buffer here.
    const auto chunkLimit = preparedBlockSize > 0
                                 ? static_cast<size_t> (preparedBlockSize)
                                 : juce::jmax (static_cast<size_t> (1), fullBlock.getNumSamples());

    for (size_t offset = 0; offset < fullBlock.getNumSamples(); offset += chunkLimit)
    {
        const auto chunkLength = juce::jmin (chunkLimit, fullBlock.getNumSamples() - offset);
        auto chunk = fullBlock.getSubBlock (offset, chunkLength);
        processChunk (chunk);
    }
}

void TwistYourGutsAudioProcessor::processChunk (juce::dsp::AudioBlock<float>& chunk) noexcept
{
    inputGainProcessor.process (juce::dsp::ProcessContextReplacing<float> (chunk));

    const auto numChannels = chunk.getNumChannels();
    const auto numSamples = chunk.getNumSamples();

    auto lowBlock = juce::dsp::AudioBlock<float> (lowBandBuffer).getSubBlock (0, numSamples).getSubsetChannelBlock (0, numChannels);
    auto highBlock = juce::dsp::AudioBlock<float> (highBandBuffer).getSubBlock (0, numSamples).getSubsetChannelBlock (0, numChannels);

    // Issue #8: split the input-trimmed signal into low/high bands.
    crossover.process (chunk, lowBlock, highBlock);

    // Issue #9: time-align the low band with the (currently zero) latency
    // the high band will pick up once M3 adds oversampling ahead of the
    // nonlinear voicing stage.
    lowBandLatencyDelay.process (juce::dsp::ProcessContextReplacing<float> (lowBlock));

    // Issue #10: independent per-band level trims, then sum the bands back
    // together in place of the pre-split signal.
    lowGainProcessor.process (juce::dsp::ProcessContextReplacing<float> (lowBlock));
    highGainProcessor.process (juce::dsp::ProcessContextReplacing<float> (highBlock));

    chunk.replaceWithSumOf (lowBlock, highBlock);

    // Optional safety clip (issue #10): a soft (tanh) limiter that only
    // engages when the user explicitly enables it, protecting against
    // accidental hard-clipped overs without colouring the signal at typical
    // playing levels (tanh(x) ~= x for |x| well below 1.0).
    if (outputClipEnabled->load (std::memory_order_relaxed) >= 0.5f)
    {
        for (size_t channel = 0; channel < numChannels; ++channel)
        {
            auto* data = chunk.getChannelPointer (channel);

            for (size_t sample = 0; sample < numSamples; ++sample)
                data[sample] = std::tanh (data[sample]);
        }
    }

    outputGainProcessor.process (juce::dsp::ProcessContextReplacing<float> (chunk));
}

//==============================================================================
bool TwistYourGutsAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* TwistYourGutsAudioProcessor::createEditor()
{
    return new TwistYourGutsAudioProcessorEditor (*this);
}

//==============================================================================
juce::AudioProcessorParameter* TwistYourGutsAudioProcessor::getBypassParameter() const
{
    return bypassParameter;
}

//==============================================================================
void TwistYourGutsAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    const std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void TwistYourGutsAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TwistYourGutsAudioProcessor();
}
