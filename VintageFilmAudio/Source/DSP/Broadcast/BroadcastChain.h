#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/StreamSpec.h"
#include "../Core/ParamSnapshot.h"
#include "../Core/Filters.h"
#include "../Utilities/OversampledStage.h"

namespace vfa::dsp
{

// Transmitter/receiver stage for broadcast media (DSP_SPEC §7). Not the
// dynamics (those live in PeriodDynamics) — this is the RF-chain coloration:
//   75us-style pre-emphasis -> soft clip (over-deviation control, oversampled)
//   -> de-emphasis -> receiver IF ripple (+-1 dB) -> MTS stereo behavior
//   (broadcastStereo: mild L-R HF loss).  HIST-APPROX throughout.
class BroadcastChain
{
public:
    void prepare (const StreamSpec& spec);
    void reset();

    // intensity 0..1 (0 = transparent). Applied for broadcast media and the
    // off-air / consumer conditions.
    void process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap,
                  float intensity);

    float latencySamples (Quality q) const noexcept { return os.latencySamples (q); }

private:
    struct ChannelState
    {
        Biquad preEmph, deEmph;   // shelving pair approximating 75 us
        Biquad ripple1, ripple2;  // small peaking ripple in the IF band
        Biquad bandHp, bandLp;    // DSP_SPEC §3 band limit per broadcast medium
        DcBlocker dc;
    };

    StreamSpec streamSpec;
    std::vector<ChannelState> channels;
    Biquad sideHfLoss;            // L-R path HF loss for MTS mode
    OversampledStage os;

    // Block-rate smoothed intensity and caches so biquad coefficients are
    // only recomputed on a real change.
    float smoothedIntensity = -1.0f;    // sentinel: snapped on first block
    float cachedIntensity   = -1.0f;
    int   cachedBand        = -1;       // 0 = mono corners, 1 = stereo corners
};

} // namespace vfa::dsp
