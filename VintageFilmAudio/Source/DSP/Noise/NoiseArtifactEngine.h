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
        double unitsToNext = -1.0;  // exponential units to next fire (<0 = unprimed)
        int remaining = 0;          // samples left in the running event
        int length = 0, pos = 0;    // shaped-event geometry (dirt/dropout)
        int gap = 0, clusterLeft = 0; // dropout cluster bookkeeping
        float amp = 0, amp2 = 0;    // ring / click amplitudes
        float phase = 0, phaseInc = 0, decay = 1;
        float depthLin = 1.0f;      // dropout floor gain
        bool primeLp = false;       // prime the dropout LP at dip start
        OnePoleLP lp;               // dirt click shaper (cutoff drawn per event)
    };

    struct ChannelState
    {
        Rng hissRng, cellRng, bcastRng, crackleRng, dirtRng, dropRng, microRng;
        OnePoleLP hissDarkLp;       // ~3 kHz darkening pole (HIST-APPROX -3 dB/oct)
        Biquad hissCellShelf;       // optical-media hiss uses the cell spectrum
        Biquad cellShelf;           // +6 dB @ 6 kHz 'blue-ish' shelf
        float hissBcPrev = 0;       // broadcast-tilt diff memory (hiss floor)
        float bcastPrev = 0;        // broadcast-tilt diff memory (nsBroadcast)
        OnePoleLP dropoutLp;        // HF loss during dropouts
        EventState crackle, dirt, dropout, micro;
    };

    struct HumState                 // correlated across channels
    {
        double phase = 0.0;         // hum fundamental, cycles 0..1
        double buzzPhase = 0.0, buzzAmPhase = 0.0;
        double cellAmPhase = 0.0;   // 96 Hz sprocket AM (cell noise)
        double projPhase = 0.0, projAmPhase = 0.0;
        Rng projRng;                // projector bed (correlated)
        Rng hissShared, cellShared, bcastShared; // width-mix shared streams
        Biquad projBp;              // 500 Hz-2 kHz projector noise bed
        OnePoleLP projThumpLp;      // 24 Hz thump softening
    };

    struct PrintThroughState
    {
        std::vector<float> delay;   // ~400 ms post-echo line per channel
        int writePos = 0;
        OnePoleLP lp;
    };

    struct Cal                      // 1/RMS of each spectral chain (prepare-time)
    {
        float hissDark = 1, hissOptical = 1, hissBroadcast = 1,
              cell = 1, broadcast = 1, projector = 1;
    };

    void reseedStreams() noexcept;
    void resetRuntimeState();
    float crackleSample (ChannelState& c, double rate, float topDb) noexcept;
    float dirtSample (ChannelState& c, double rate, float topDb) noexcept;
    float microSample (ChannelState& c, double rate) noexcept;
    float processDropout (ChannelState& c, float x, double rate, float amount) noexcept;

    StreamSpec streamSpec;
    std::vector<ChannelState> channels;
    std::vector<PrintThroughState> printThrough;
    HumState hum;
    EnvelopeFollower programEnv;    // for ducking (mono sum, pre-noise)
    Cal cal;
    float bcastDiffK = 1.0f;        // +6 dB/oct-above-3 kHz differentiator gain
    // ~50 ms one-pole smoothing on every level control (no zipper).
    OnePoleLP smHiss, smCell, smBcast, smHum, smBuzz, smProj, smPrint,
              smDuck, smWidth;
    uint64_t seed = 1;
    int printDelaySamples = 0;
};

} // namespace vfa::dsp
