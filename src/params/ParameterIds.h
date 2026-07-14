#pragma once

// Central definition of all AudioProcessorValueTreeState parameter IDs.
// Keeping these in one place avoids typo-mismatches between the layout
// creation code, the processor's parameter lookups, and any future GUI code.
//
// FROZEN AS OF THE M1 FULL v1.0 PARAMETER LAYOUT (issue #7):
// Parameter IDs below must NEVER change once shipped - saved sessions and
// presets persist the APVTS state keyed by these string IDs, and renaming or
// removing one would silently break every user's saved state. Ranges,
// defaults, skew and display labels MAY still be refined during voicing/
// tuning milestones (M2-M4); only the IDs themselves are frozen.
//
// Several parameters declared here (everything below "IO / Global") are
// declared-but-inert as of M1: they exist in the APVTS layout and are fully
// covered by tests, but processBlock() does not yet read them. They are
// wired into the signal chain by their respective milestones (M2 dynamics,
// M3 distortion, M4 EQ/IR).

namespace ParamIDs
{
    //==============================================================================
    // IO / Global
    inline constexpr auto inputGain = "inputGain";
    inline constexpr auto outputGain = "outputGain";
    inline constexpr auto bypass = "bypass";
    inline constexpr auto outputClip = "outputClip";

    //==============================================================================
    // Noise gate (full-band, pre-crossover)
    inline constexpr auto gateEnabled = "gateEnabled";
    inline constexpr auto gateThreshold = "gateThreshold";
    inline constexpr auto gateRatio = "gateRatio";
    inline constexpr auto gateAttack = "gateAttack";
    inline constexpr auto gateRelease = "gateRelease";

    //==============================================================================
    // Crossover (Linkwitz-Riley 4th order split point)
    inline constexpr auto crossoverFreq = "crossoverFreq";

    //==============================================================================
    // Low band: compressor + level
    inline constexpr auto lowCompThreshold = "lowCompThreshold";
    inline constexpr auto lowCompRatio = "lowCompRatio";
    inline constexpr auto lowCompAttack = "lowCompAttack";
    inline constexpr auto lowCompRelease = "lowCompRelease";
    inline constexpr auto lowCompMakeup = "lowCompMakeup";
    inline constexpr auto lowCompMix = "lowCompMix";
    inline constexpr auto lowLevel = "lowLevel";

    //==============================================================================
    // High band: voicing + drive + tone + blend + level
    inline constexpr auto highVoicing = "highVoicing";
    inline constexpr auto highDrive = "highDrive";
    inline constexpr auto highTone = "highTone";
    inline constexpr auto highBlend = "highBlend";
    inline constexpr auto highLevel = "highLevel";

    //==============================================================================
    // Post-sum 4-band EQ (LowShelf / Peak / Peak / HighShelf)
    inline constexpr auto eqEnabled = "eqEnabled";
    inline constexpr auto eqLowShelfFreq = "eqLowShelfFreq";
    inline constexpr auto eqLowShelfGain = "eqLowShelfGain";
    inline constexpr auto eqPeak1Freq = "eqPeak1Freq";
    inline constexpr auto eqPeak1Gain = "eqPeak1Gain";
    inline constexpr auto eqPeak1Q = "eqPeak1Q";
    inline constexpr auto eqPeak2Freq = "eqPeak2Freq";
    inline constexpr auto eqPeak2Gain = "eqPeak2Gain";
    inline constexpr auto eqPeak2Q = "eqPeak2Q";
    inline constexpr auto eqHighShelfFreq = "eqHighShelfFreq";
    inline constexpr auto eqHighShelfGain = "eqHighShelfGain";

    //==============================================================================
    // IR loader / cab sim (the IR file path itself is non-parameter state,
    // handled separately in M4)
    inline constexpr auto irEnabled = "irEnabled";
    inline constexpr auto irMix = "irMix";
}
