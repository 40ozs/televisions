#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "StreamSpec.h"

namespace vfa::dsp
{

// Click-free enable/bypass for a processing stage. Owns a 10 ms equal-power
// crossfade between the stage's input (dry) and output (wet). While fully
// bypassed the wrapped stage is skipped entirely (no CPU); while fully active
// no dry copy is made. All buffers preallocated in prepare().
class ModuleSlot
{
public:
    void prepare (const StreamSpec& spec)
    {
        dry.setSize (spec.numChannels, spec.maxBlockSize, false, false, true);
        const float fadeSamples = float (spec.sampleRate) * 0.010f;
        step = fadeSamples > 1.0f ? 1.0f / fadeSamples : 1.0f;
        fade = target;
    }

    void reset() noexcept { fade = target; }

    void setEnabled (bool shouldBeEnabled) noexcept
    {
        target = shouldBeEnabled ? 1.0f : 0.0f;
    }

    bool isFullyBypassed() const noexcept { return fade <= 0.0f && target <= 0.0f; }

    // processFn takes (juce::AudioBuffer<float>&) and processes in place.
    template <typename Fn>
    void process (juce::AudioBuffer<float>& buffer, Fn&& processFn)
    {
        const int numCh = buffer.getNumChannels();
        const int n = buffer.getNumSamples();

        if (isFullyBypassed())
            return;

        const bool fading = fade < 1.0f || target < 1.0f;
        if (fading)
            for (int ch = 0; ch < numCh && ch < dry.getNumChannels(); ++ch)
                dry.copyFrom (ch, 0, buffer, ch, 0, n);

        processFn (buffer);

        if (fading)
        {
            for (int i = 0; i < n; ++i)
            {
                fade += (target - fade > 0.0f ? step : (target - fade < 0.0f ? -step : 0.0f));
                if (std::abs (fade - target) < step) fade = target;
                const float wetGain = fade;
                const float dryGain = 1.0f - fade;
                for (int ch = 0; ch < numCh && ch < dry.getNumChannels(); ++ch)
                {
                    auto* w = buffer.getWritePointer (ch);
                    const auto* d = dry.getReadPointer (ch);
                    w[i] = w[i] * wetGain + d[i] * dryGain;
                }
            }
        }
    }

private:
    juce::AudioBuffer<float> dry;
    float fade = 1.0f, target = 1.0f, step = 0.01f;
};

} // namespace vfa::dsp
