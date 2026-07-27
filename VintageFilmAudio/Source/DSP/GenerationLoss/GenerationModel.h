#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/StreamSpec.h"
#include "../Core/ParamSnapshot.h"
#include "../Core/Filters.h"
#include "../Core/Rng.h"

namespace vfa::dsp
{

// Accumulated transfer-generation model (DSP_SPEC §6, ADR-007).
// This module applies the accumulated HF loss + transient softening +
// seeded per-generation variability tilt. The other accumulation effects are
// *published* to the rest of the chain via the static helpers:
//   noiseLiftDb()      -> NoiseArtifactEngine floor lift (incoherent sum)
//   driveLiftDb()      -> medium nonlinearity drive increase (soft-capped)
//   wowFlutterScale()  -> transport depth multiplier (sqrt accumulation)
// Fractional generation counts interpolate continuously; genInteger snaps.
// Tests validate against ExactReferenceChain (repeated literal stages).
class GenerationModel
{
public:
    void prepare (const StreamSpec& spec);
    void reset();
    void setSeed (uint64_t resolvedSeed);

    void process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap);

    // Effective (possibly snapped) generation count.
    static float effectiveGenerations (const ParamSnapshot& snap) noexcept;

    // Per-medium single-transfer HF prototype cutoff (DSP_SPEC §6).
    static float prototypeCutoffHz (Medium m) noexcept;

    // N-fold accumulated -3 dB cutoff of the one-pole prototype.
    static float accumulatedCutoffHz (Medium m, float generations) noexcept;

    static float noiseLiftDb (const ParamSnapshot& snap) noexcept;
    static float driveLiftDb (const ParamSnapshot& snap) noexcept;
    static float wowFlutterScale (const ParamSnapshot& snap) noexcept;

private:
    struct ChannelState
    {
        OnePoleLP hf1, hf2;          // cascaded accumulated HF loss
        Biquad tiltCorrect;          // keeps mids near unity as N rises
        Biquad variabilityTilt;      // seeded per-generation tolerance tilt
        EnvelopeFollower attackEnv;  // transient softening detector
        OnePoleLP softenLp;
    };

    StreamSpec streamSpec;
    std::vector<ChannelState> channels;
    Rng rng;
    float variabilityTiltDb = 0.0f;  // recomputed when seed/N changes
    float lastGenApplied = -1.0f;
    uint64_t seed = 1;
};

} // namespace vfa::dsp
