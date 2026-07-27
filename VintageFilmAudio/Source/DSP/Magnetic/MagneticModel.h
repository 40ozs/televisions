#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/StreamSpec.h"
#include "../Core/ParamSnapshot.h"
#include "../Core/Filters.h"
#include "../Utilities/OversampledStage.h"

namespace vfa::dsp
{

// First-pass magnetic film / tape model (DSP_SPEC §3, post-MVP replacement
// path: Jiles-Atherton behind this same interface). Chain:
//   head-bump peaking (freq tracks tape speed) -> level-dependent HF loss
//   (drive-scaled one-pole before the shaper, inverse tilt after)
//   -> odd-order-dominant saturator (oversampled) -> gap/azimuth HF shelf.
// Medium variants (magneticFilm / fieldTape / consumer) select bandwidth,
// bump and saturation calibration internally from snap.medium.
class MagneticModel
{
public:
    void prepare (const StreamSpec& spec);
    void reset();

    void process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap,
                  float extraDriveDb);

    float latencySamples (Quality q) const noexcept { return os.latencySamples (q); }

    // Head-bump centre frequency for a speed (HIST-APPROX, DSP_SPEC §3).
    static float headBumpHzFor (TapeSpeed s) noexcept;

private:
    struct ChannelState
    {
        Biquad headBump;
        OnePoleLP hfLossPre;     // level/drive-dependent pre-shaper loss
        Biquad hfRestore;        // partial inverse tilt post-shaper
        Biquad gapLoss;          // fixed top-end shelf per medium variant
        Biquad azimuthLoss;      // consumer-only fixed azimuth-style HF shelf
        Biquad lowCut;           // lower band edge per medium variant
        EnvelopeFollower levelEnv;
        DcBlocker dc;
    };

    StreamSpec streamSpec;
    std::vector<ChannelState> channels;
    OversampledStage os;

    // Block-rate smoothed controls (zipper-free drive/EQ moves) and caches so
    // biquad coefficients are only recomputed on a real change.
    float smoothedDriveDb = -1000.0f;   // sentinel: snapped on first block
    float smoothedBumpDb  = 0.0f;
    int   cachedVariant   = -1;
    int   cachedSpeed     = -1;
    float cachedBumpDb    = -1000.0f;
    float cachedRestoreDb = -1000.0f;
};

} // namespace vfa::dsp
