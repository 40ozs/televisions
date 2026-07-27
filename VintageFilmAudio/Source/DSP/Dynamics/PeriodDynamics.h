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
    // AGC: +-12 dB range, 300 ms attack, 1.5-4 s release, gated < -45 dBFS.
    EnvelopeFollower agcDetector;
    float agcGain = 1.0f;

    // Compressor/limiter shared detector state.
    EnvelopeFollower compDetector, limitDetector;
    float compGain = 1.0f, limitGain = 1.0f, slowRelease = 1.0f;

    struct ChannelEq { Biquad presence, lowCut; };
    std::vector<ChannelEq> eq;

    StreamSpec streamSpec;
    float grDb = 0.0f;
};

} // namespace vfa::dsp
