#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/StreamSpec.h"
#include "../Core/ParamSnapshot.h"
#include "../Core/Filters.h"
#include "../Core/Rng.h"

namespace vfa::dsp
{

// Wow/flutter/drift/scrape speed-instability engine (DSP_SPEC §5, ADR-006).
// One bounded fractional-delay line per channel (cubic Hermite) around a
// FIXED centre delay that is ALWAYS in circuit (constant plugin latency; at
// zero depth the line passes audio at exactly the centre delay).
//
// Modulator = drift (leaky-integrated bounded random walk)
//           + wow (two quasi-periodic components with jittered rate/amp)
//           + flutter (narrowband noise + motor component)
//           + scrape (small high-band noise FM).
// Generated at control rate (32 samples) and linearly interpolated —
// no zipper, hard excursion bound by construction.
// Deterministic: all streams derive from Rng::streamSeed (ADR-016);
// stereoLink shares one modulator across channels.
class WowFlutterEngine
{
public:
    static constexpr float centreDelayMs = 12.0f;
    static constexpr float maxExcursionMs = 10.0f;   // hard bound (< centre)
    static constexpr int controlInterval = 32;       // samples

    void prepare (const StreamSpec& spec);
    void reset();
    void setSeed (uint64_t resolvedSeed);            // prepare/message thread

    // depthScale: generation-loss accumulation multiplier (>= 1).
    void process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap,
                  float depthScale);

    float latencySamples() const noexcept { return centreDelaySamples; }

    // Speed deviation calibration (DSP_SPEC §5): param 0.5 -> 0.1 %,
    // param 1.0 -> 0.4 % peak speed deviation (quadratic map).
    static float depthToSpeedFraction (float param01) noexcept;

private:
    struct ModulatorState
    {
        Rng rng;
        float driftState = 0, driftTarget = 0;
        float wowPhase1 = 0, wowPhase2 = 0, wowJitter = 0;
        float flutterPhase = 0;
        OnePoleLP flutterNoiseLp, scrapeNoiseBp, driftLp, jitterLp;
        float lastValueMs = 0, nextValueMs = 0;      // control-rate endpoints
    };

    struct ChannelState
    {
        std::vector<float> delayLine;
        int writePos = 0;
        ModulatorState mod;                          // used when unlinked
    };

    float computeControlValueMs (ModulatorState& m, const ParamSnapshot& snap,
                                 float depthScale) noexcept;

    StreamSpec streamSpec;
    std::vector<ChannelState> channels;
    ModulatorState linkedMod;
    float centreDelaySamples = 0;
    int delayLineLength = 0;
    int controlCountdown = 0;
    uint64_t baseSeed = 1;
};

} // namespace vfa::dsp
