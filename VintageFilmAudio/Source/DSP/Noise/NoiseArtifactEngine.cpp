// STUB — replaced in Phase 5 (real noise/artifact engine per DSP_SPEC §8).
#include "NoiseArtifactEngine.h"

namespace vfa::dsp
{
void NoiseArtifactEngine::prepare (const StreamSpec& spec)
{
    streamSpec = spec;
    channels.assign ((size_t) spec.numChannels, {});
    printThrough.assign ((size_t) spec.numChannels, {});
    printDelaySamples = (int) (0.4 * spec.sampleRate);
    for (auto& p : printThrough) p.delay.assign ((size_t) printDelaySamples + 1, 0.0f);
    programEnv.prepare (spec.sampleRate, 5.0f, 200.0f);
    setSeed (seed);
}

void NoiseArtifactEngine::reset()
{
    for (auto& p : printThrough) { std::fill (p.delay.begin(), p.delay.end(), 0.0f); p.writePos = 0; }
    programEnv.reset();
}

void NoiseArtifactEngine::setSeed (uint64_t s) { seed = s == 0 ? 1 : s; }

void NoiseArtifactEngine::process (juce::AudioBuffer<float>&, const ParamSnapshot&, float) {}
} // namespace vfa::dsp
