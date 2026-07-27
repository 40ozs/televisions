// STUB — replaced in Phase 3 (real broadcast chain per DSP_SPEC §7).
#include "BroadcastChain.h"

namespace vfa::dsp
{
void BroadcastChain::prepare (const StreamSpec& spec)
{
    streamSpec = spec;
    channels.assign ((size_t) spec.numChannels, {});
    for (auto& c : channels) c.dc.prepare (spec.sampleRate);
    os.prepare (spec);
}

void BroadcastChain::reset()
{
    for (auto& c : channels) { c.preEmph.reset(); c.deEmph.reset(); c.ripple1.reset(); c.ripple2.reset(); c.dc.reset(); }
    sideHfLoss.reset();
    os.reset();
}

void BroadcastChain::process (juce::AudioBuffer<float>&, const ParamSnapshot&, float) {}
} // namespace vfa::dsp
