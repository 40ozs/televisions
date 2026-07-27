#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/StreamSpec.h"
#include "../Core/ParamSnapshot.h"
#include "../Core/Filters.h"

namespace vfa::dsp
{

// Reproduction-system stage (DSP_SPEC §3/§7): width control and mono
// fold-down (selectable sum law), then the small-speaker model — band
// limiting with cabinet resonance and a gentle excursion nonlinearity,
// morphed in by smallSpeaker amount. NOT a telephone band-pass: response
// targets (BroadcastTest) keep 150 Hz and 6 kHz alive at moderate settings.
class Reproduction
{
public:
    void prepare (const StreamSpec& spec);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap);

private:
    struct ChannelState
    {
        Biquad speakerHp, speakerLp, cabinetRes, presence;
        DcBlocker dc;
    };

    StreamSpec streamSpec;
    std::vector<ChannelState> channels;
    OnePoleLP widthSmoother, monoSmoother, speakerSmoother;
};

} // namespace vfa::dsp
