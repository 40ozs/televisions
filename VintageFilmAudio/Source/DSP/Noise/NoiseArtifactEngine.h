#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/StreamSpec.h"
#include "../Core/ParamSnapshot.h"
#include "../Core/Filters.h"
#include "../Core/Rng.h"

namespace vfa::dsp
{

// Noise & mechanical-artifact engine (DSP_SPEC §8, ADR-008/016).
// Everything is procedurally generated per sample from seeded Rng streams —
// no looped buffers anywhere. Events (crackle/dirt/dropout/microphonic) are
// Poisson-scheduled with seeded draws. Media noise persists in silence when
// nsSilence is on; program-dependent ducking applies to continuous noise
// only (never to impulsive artifacts). Adds into the buffer in place.
//
// Calibration (HIST-APPROX): each continuous source at param=0.5 sits near
// the SNR target of its medium (DSP_SPEC §3) relative to -18 dBFS nominal.
class NoiseArtifactEngine
{
public:
    void prepare (const StreamSpec& spec);
    void reset();
    void setSeed (uint64_t resolvedSeed);

    // extraNoiseDb: generation-loss floor lift. auditionArtifactsOnly:
    // caller zeroed the program; engine must still generate identically.
    void process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap,
                  float extraNoiseDb);

private:
    struct EventState               // Poisson-scheduled artifact generator
    {
        int samplesToNext = 0;
        int remaining = 0;          // samples left in the running event
        float amp = 0, phase = 0, freq = 0, decay = 1;
        int shapePos = 0, shapeLen = 0;
    };

    struct ChannelState
    {
        Rng hissRng, cellRng, crackleRng, dirtRng, dropRng, microRng;
        OnePoleLP hissTilt;         // -3 dB/oct-ish tilt via one-pole mix
        Biquad hissShape, cellShape, broadcastShape;
        Biquad crackleRing;
        OnePoleLP dropoutLp;        // HF loss during dropouts
        EventState crackle, dirt, dropout, micro;
        float dropoutGain = 1.0f;
    };

    struct HumState                 // correlated across channels
    {
        float phase = 0.0f;         // fundamental phase 0..1
        float buzzPhase = 0.0f;     // frame-rate AM phase
        float projPhase = 0.0f;     // 24 Hz gate phase
        Rng projRng;
        OnePoleLP projNoiseLp;
    };

    struct PrintThroughState
    {
        std::vector<float> delay;   // ~400 ms post-echo line per channel
        int writePos = 0;
        OnePoleLP lp;
    };

    StreamSpec streamSpec;
    std::vector<ChannelState> channels;
    std::vector<PrintThroughState> printThrough;
    HumState hum;
    EnvelopeFollower programEnv;    // for ducking (mono sum, pre-noise)
    uint64_t seed = 1;
    int printDelaySamples = 0;
};

} // namespace vfa::dsp
