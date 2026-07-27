// STUB — replaced in Phase 3 (real magnetic model per DSP_SPEC §3).
#include "MagneticModel.h"

namespace vfa::dsp
{
float MagneticModel::headBumpHzFor (TapeSpeed s) noexcept
{
    switch (s)
    {
        case TapeSpeed::ips3_75: return 90.0f;
        case TapeSpeed::ips7_5:  return 70.0f;
        case TapeSpeed::ips15:   return 55.0f;
        case TapeSpeed::ips30:   return 40.0f;
    }
    return 55.0f;
}

void MagneticModel::prepare (const StreamSpec& spec)
{
    streamSpec = spec;
    channels.assign ((size_t) spec.numChannels, {});
    for (auto& c : channels) { c.levelEnv.prepare (spec.sampleRate, 5.0f, 50.0f); c.dc.prepare (spec.sampleRate); }
    os.prepare (spec);
}

void MagneticModel::reset()
{
    for (auto& c : channels) { c.headBump.reset(); c.hfLossPre.reset(); c.hfRestore.reset(); c.gapLoss.reset(); c.levelEnv.reset(); c.dc.reset(); }
    os.reset();
}

void MagneticModel::process (juce::AudioBuffer<float>&, const ParamSnapshot&, float) {}
} // namespace vfa::dsp
