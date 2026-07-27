// STUB — replaced in Phase 4 (real wow/flutter per DSP_SPEC §5).
#include "WowFlutterEngine.h"

namespace vfa::dsp
{
float WowFlutterEngine::depthToSpeedFraction (float p) noexcept
{
    const float c = std::clamp (p, 0.0f, 1.0f);
    return 0.004f * c * c; // 0.5 -> 0.1 %, 1.0 -> 0.4 %
}

void WowFlutterEngine::prepare (const StreamSpec& spec)
{
    streamSpec = spec;
    centreDelaySamples = centreDelayMs * 0.001f * (float) spec.sampleRate;
    delayLineLength = (int) std::ceil ((centreDelayMs + maxExcursionMs + 4.0f) * 0.001 * spec.sampleRate);
    channels.assign ((size_t) spec.numChannels, {});
    for (auto& c : channels) c.delayLine.assign ((size_t) delayLineLength, 0.0f);
    controlCountdown = 0;
    setSeed (baseSeed);
}

void WowFlutterEngine::reset()
{
    for (auto& c : channels)
    {
        std::fill (c.delayLine.begin(), c.delayLine.end(), 0.0f);
        c.writePos = 0;
    }
}

void WowFlutterEngine::setSeed (uint64_t s) { baseSeed = s == 0 ? 1 : s; }

float WowFlutterEngine::computeControlValueMs (ModulatorState&, const ParamSnapshot&, float) noexcept { return 0.0f; }

void WowFlutterEngine::process (juce::AudioBuffer<float>& buffer, const ParamSnapshot&, float)
{
    // Stub: fixed centre delay only (constant latency), integer taps.
    const int numCh = std::min (buffer.getNumChannels(), (int) channels.size());
    const int n = buffer.getNumSamples();
    const int delay = (int) centreDelaySamples;
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& c = channels[(size_t) ch];
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            c.delayLine[(size_t) c.writePos] = d[i];
            int rp = c.writePos - delay;
            if (rp < 0) rp += delayLineLength;
            d[i] = c.delayLine[(size_t) rp];
            if (++c.writePos >= delayLineLength) c.writePos = 0;
        }
    }
}
} // namespace vfa::dsp
