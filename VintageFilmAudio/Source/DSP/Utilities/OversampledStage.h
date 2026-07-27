#pragma once

#include <memory>
#include <juce_dsp/juce_dsp.h>
#include "../Core/StreamSpec.h"
#include "../Core/Enums.h"

namespace vfa::dsp
{

// Oversampling wrapper scoped to a single nonlinear stage (ADR-009).
// Eco = 1x (no oversampling), Standard = 2x, High = 4x. Both oversamplers are
// preallocated in prepare(); switching quality never allocates (ADR-010).
class OversampledStage
{
public:
    void prepare (const StreamSpec& spec)
    {
        using OS = juce::dsp::Oversampling<float>;
        const auto ch = (size_t) spec.numChannels;
        os2 = std::make_unique<OS> (ch, 1, OS::filterHalfBandPolyphaseIIR, false);
        os4 = std::make_unique<OS> (ch, 2, OS::filterHalfBandPolyphaseIIR, false);
        os2->initProcessing ((size_t) spec.maxBlockSize);
        os4->initProcessing ((size_t) spec.maxBlockSize);
        baseRate = spec.sampleRate;
    }

    void reset()
    {
        if (os2) os2->reset();
        if (os4) os4->reset();
    }

    float latencySamples (Quality q) const noexcept
    {
        if (q == Quality::standard && os2) return (float) os2->getLatencyInSamples();
        if (q == Quality::high && os4)     return (float) os4->getLatencyInSamples();
        return 0.0f;
    }

    // fn (float* const* channelData, int numChannels, int numSamples, double rate)
    template <typename Fn>
    void process (juce::AudioBuffer<float>& buffer, Quality q, Fn&& fn)
    {
        const int numCh = buffer.getNumChannels();
        const int n = buffer.getNumSamples();

        if (q == Quality::eco || (os2 == nullptr && os4 == nullptr))
        {
            fn (buffer.getArrayOfWritePointers(), numCh, n, baseRate);
            return;
        }

        auto& os = (q == Quality::high) ? *os4 : *os2;
        const double factor = (q == Quality::high) ? 4.0 : 2.0;

        juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(),
                                            (size_t) numCh, (size_t) n);
        auto up = os.processSamplesUp (block);

        float* chans[8] = {};
        const int upCh = (int) up.getNumChannels();
        for (int c = 0; c < upCh && c < 8; ++c)
            chans[c] = up.getChannelPointer ((size_t) c);
        fn (chans, upCh, (int) up.getNumSamples(), baseRate * factor);

        os.processSamplesDown (block);
    }

private:
    std::unique_ptr<juce::dsp::Oversampling<float>> os2, os4;
    double baseRate = 48000.0;
};

} // namespace vfa::dsp
