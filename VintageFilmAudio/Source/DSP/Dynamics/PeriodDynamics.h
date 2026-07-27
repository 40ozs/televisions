#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/StreamSpec.h"
#include "../Core/ParamSnapshot.h"
#include "../Core/Filters.h"

namespace vfa::dsp
{

// Period dynamics (DSP_SPEC §7): broadcast AGC -> dialogue-focus shaping ->
// program compressor -> peak limiter. Gain computers run on a linked
// (max-of-channels) detector so stereo never wanders. Release Character
// morphs vari-mu-flavoured (slow, soft-knee, program-dependent dual release)
// toward FET-flavoured (fast, harder knee).
class PeriodDynamics
{
public:
    void prepare (const StreamSpec& spec);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap);

    float currentGainReductionDb() const noexcept { return grDb; }

private:
    // AGC (§7): gain rider toward the -18 dBFS nominal, bounded +-12 dB * agc,
    // gated below -45 dBFS (rider frozen -> no noise suck-up). The rider slews
    // 300 ms toward cuts and 1.5-4 s (pump-shortened) toward boosts; the
    // applied linear gain gets a further ~100 ms one-pole on top.
    EnvelopeFollower agcDetector;   // program-level tracker (50 ms / 300 ms)
    EnvelopeFollower agcGate;       // fast gate envelope (2 ms / 50 ms)
    float agcRiderDb = 0.0f, agcGain = 1.0f;
    float agcAttackCoeff = 0.0f, agcSmoothCoeff = 0.0f;

    // Compressor / limiter: channel-linked detectors; dB-domain gain
    // computers with peak-hold slow-release components (vari-mu tail).
    EnvelopeFollower compDetector, limitDetector;
    float compGain = 1.0f, limitGain = 1.0f;
    float compGrSlowDb = 0.0f, limitGrSlowDb = 0.0f;
    float compSlowRelCoeff = 0.0f, limitSlowRelCoeff = 0.0f;
    float gainSmoothCoeff = 0.0f;

    // Crest-factor tracking: program-dependent blend of the dual releases.
    EnvelopeFollower crestPeak;
    float crestRmsSq = 0.0f, crestRmsCoeff = 0.0f, slowWeight = 1.0f;

    // Dialogue-focus EQ (coefficients cached against `dialog`).
    struct ChannelEq { Biquad presence, lowCut; };
    std::vector<ChannelEq> eq;
    float lastDialog = -1.0f;

    StreamSpec streamSpec;
    float grDb = 0.0f;    // positive dB: current comp + limiter reduction
};

} // namespace vfa::dsp
