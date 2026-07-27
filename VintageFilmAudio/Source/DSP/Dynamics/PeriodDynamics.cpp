// STUB — replaced in Phase 6 (real dynamics per DSP_SPEC §7).
#include "PeriodDynamics.h"

namespace vfa::dsp
{
void PeriodDynamics::prepare (const StreamSpec& spec)
{
    streamSpec = spec;
    agcDetector.prepare (spec.sampleRate, 300.0f, 2000.0f);
    compDetector.prepare (spec.sampleRate, 10.0f, 200.0f);
    limitDetector.prepare (spec.sampleRate, 1.0f, 80.0f);
    eq.assign ((size_t) spec.numChannels, {});
    agcGain = compGain = limitGain = 1.0f;
    grDb = 0.0f;
}

void PeriodDynamics::reset()
{
    agcDetector.reset(); compDetector.reset(); limitDetector.reset();
    for (auto& e : eq) { e.presence.reset(); e.lowCut.reset(); }
    agcGain = compGain = limitGain = 1.0f;
    grDb = 0.0f;
}

void PeriodDynamics::process (juce::AudioBuffer<float>&, const ParamSnapshot&) {}
} // namespace vfa::dsp
