#pragma once

#include <array>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/StreamSpec.h"
#include "../Core/ParamSnapshot.h"
#include "../Core/Filters.h"
#include "../Utilities/OversampledStage.h"

namespace vfa::dsp
{

// Optical soundtrack model (DSP_SPEC §4, ADR-005). Chain:
//   slit-loss LPF -> modulation-peak rounding -> HF emphasis -> VA/VD
//   transfer curve (oversampled) -> HF de-emphasis -> image-spread
//   (level-dependent HF loss) -> DC servo.
// Distinctness requirements (test-enforced): sibilance-selective distortion
// (THD @ 8 kHz >> THD @ 200 Hz at equal level) and HF content that
// *decreases* with level (image spread), unlike generic clippers.
//
// The image-spread stage sits behind IImageSpreadStage so a physically
// informed model can replace it later without touching the rest.
class IImageSpreadStage
{
public:
    virtual ~IImageSpreadStage() = default;
    virtual void prepare (const StreamSpec&) = 0;
    virtual void reset() = 0;
    // amount 0..1; processes one channel in place.
    virtual void process (float* samples, int numSamples, int channel, float amount) = 0;
};

class OpticalModel
{
public:
    OpticalModel();
    ~OpticalModel();

    void prepare (const StreamSpec& spec);
    void reset();

    // intensity 0..1 scales the whole model (kinescope uses a mild setting);
    // extraDriveDb comes from the generation-loss model.
    void process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap,
                  float intensity, float extraDriveDb);

    float latencySamples (Quality q) const noexcept { return os.latencySamples (q); }

private:
    struct ChannelState
    {
        Biquad slit1, slit2;          // slit-loss rolloff (Gaussian-ish, 2 biquads)
        Biquad preEmph, deEmph;       // +/-10 dB shelf @ 4 kHz around the shaper
        EnvelopeFollower peakEnv;     // modulation-peak rounding detector
        DcBlocker dc;
        float roundGain = 1.0f;       // modulation-peak rounding rider state
        std::array<float, 64> dryRing {};  // latency-matched dry line for blend
        int dryPos = 0;
    };

    void designSlit (float cornerHz);
    void designEmphasis (float shelfDb);

    StreamSpec streamSpec;
    std::vector<ChannelState> channels;
    std::unique_ptr<IImageSpreadStage> imageSpread;
    OversampledStage os;
    juce::AudioBuffer<float> scratch;

    // Smoothed control state (no zipper on snapshot jumps).
    float slitDesignedHz = -1.0f;
    float emphDesignedDb = -1.0e9f;
    float driveGain      = 1.0e-4f;
    float wetMix         = 0.0f;
    float intens         = 0.0f;
};

} // namespace vfa::dsp
