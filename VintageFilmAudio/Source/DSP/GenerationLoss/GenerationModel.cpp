// STUB — replaced in Phase 4 (real generation-loss model per DSP_SPEC §6).
#include "GenerationModel.h"
#include "../Core/MacroMath.h"

namespace vfa::dsp
{
float GenerationModel::effectiveGenerations (const ParamSnapshot& snap) noexcept
{
    const float g = std::clamp (snap.generations, 0.0f, 8.0f);
    return snap.genInteger ? std::round (g) : g;
}

float GenerationModel::prototypeCutoffHz (Medium m) noexcept
{
    switch (m)
    {
        case Medium::opticalMono:
        case Medium::kinescope:      return 9000.0f;
        case Medium::opticalStereo:  return 11000.0f;
        case Medium::magneticFilm:   return 16000.0f;
        case Medium::fieldTape:      return 14000.0f;
        case Medium::consumer:       return 10000.0f;
        case Medium::broadcastMono:  return 8000.0f;
        case Medium::broadcastStereo:return 14000.0f;
        case Medium::clean:          return 20000.0f;
    }
    return 16000.0f;
}

float GenerationModel::accumulatedCutoffHz (Medium m, float generations) noexcept
{
    // N cascaded one-poles at f0: -3 dB point ~= f0 * sqrt(2^(1/N) - 1).
    const float f0 = prototypeCutoffHz (m);
    if (generations < 0.01f) return f0 * 4.0f;
    return f0 * std::sqrt (std::pow (2.0f, 1.0f / generations) - 1.0f);
}

float GenerationModel::noiseLiftDb (const ParamSnapshot& snap) noexcept
{
    return 3.0f * std::sqrt (effectiveGenerations (snap));
}

float GenerationModel::driveLiftDb (const ParamSnapshot& snap) noexcept
{
    return std::min (1.0f * effectiveGenerations (snap), 6.0f);
}

float GenerationModel::wowFlutterScale (const ParamSnapshot& snap) noexcept
{
    return macros::generationWowFlutterScale (effectiveGenerations (snap));
}

void GenerationModel::prepare (const StreamSpec& spec)
{
    streamSpec = spec;
    channels.assign ((size_t) spec.numChannels, {});
    for (auto& c : channels) c.attackEnv.prepare (spec.sampleRate, 1.0f, 50.0f);
    lastGenApplied = -1.0f;
}

void GenerationModel::reset()
{
    for (auto& c : channels)
    { c.hf1.reset(); c.hf2.reset(); c.tiltCorrect.reset(); c.variabilityTilt.reset(); c.attackEnv.reset(); c.softenLp.reset(); }
}

void GenerationModel::setSeed (uint64_t s) { seed = s == 0 ? 1 : s; }

void GenerationModel::process (juce::AudioBuffer<float>&, const ParamSnapshot&) {}
} // namespace vfa::dsp
