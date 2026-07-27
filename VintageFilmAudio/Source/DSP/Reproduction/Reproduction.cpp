// STUB — replaced in Phase 6 (real reproduction stage per DSP_SPEC §3/§7).
#include "Reproduction.h"

namespace vfa::dsp
{
void Reproduction::prepare (const StreamSpec& spec)
{
    streamSpec = spec;
    channels.assign ((size_t) spec.numChannels, {});
    for (auto& c : channels) c.dc.prepare (spec.sampleRate);
    widthSmoother.setCutoff (8.0f, spec.sampleRate);
    monoSmoother.setCutoff (8.0f, spec.sampleRate);
    speakerSmoother.setCutoff (8.0f, spec.sampleRate);
}

void Reproduction::reset()
{
    for (auto& c : channels)
    { c.speakerHp.reset(); c.speakerLp.reset(); c.cabinetRes.reset(); c.presence.reset(); c.dc.reset(); }
    widthSmoother.reset(); monoSmoother.reset(); speakerSmoother.reset();
}

void Reproduction::process (juce::AudioBuffer<float>&, const ParamSnapshot&) {}
} // namespace vfa::dsp
