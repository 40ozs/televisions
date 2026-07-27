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
        EnvelopeFollower levelEnv;
        DcBlocker dc;
    };

    StreamSpec streamSpec;
    std::vector<ChannelState> channels;
    OversampledStage os;
};

} // namespace vfa::dsp
