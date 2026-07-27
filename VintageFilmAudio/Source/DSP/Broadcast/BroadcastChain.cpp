// Transmitter/receiver coloration stage (DSP_SPEC §7 — not the dynamics,
// those live in PeriodDynamics): 75 us-style pre-emphasis -> oversampled
// soft clip (over-deviation control) -> matching de-emphasis -> receiver IF
// ripple -> §3 band limit -> MTS L-R HF loss. The pre-emph -> clip -> de-emph
// sandwich is the point of the stage: HF-heavy content hits the deviation
// limit first, giving level-dependent HF compression. HIST-APPROX throughout;
// intensity 0..1 blends every stage toward transparent.
#include "BroadcastChain.h"

namespace vfa::dsp
{

namespace
{
// 75 us-style emphasis approximation (HIST-APPROX): +9 dB shelf @ 2.1 kHz,
// exactly inverted on the receive side so the pair is transparent until the
// clipper engages.
constexpr float kEmphHz = 2100.0f;
constexpr float kEmphDb = 9.0f;

// Over-deviation soft clip: linear to the knee, fastTanh above. The knee sits
// so nominal -18 dBFS program passes clean even with full pre-emphasis
// (0.126 * +9 dB ~= 0.355 < 0.38).
constexpr float kClipKnee = 0.38f;
constexpr float kClipSpan = 0.42f;

// Receiver IF ripple (HIST-APPROX): gentle +-1 dB peaking pair.
constexpr float kRipple1Hz = 2500.0f, kRipple1Db =  1.0f, kRipple1Q = 1.4f;
constexpr float kRipple2Hz = 7000.0f, kRipple2Db = -1.0f, kRipple2Q = 1.2f;

// DSP_SPEC §3 band limits. The lowpass corners sit slightly below the table
// values so the total chain (band limit + ripple) measures -3 dB there.
constexpr float kMonoLowHz   = 100.0f, kMonoHighHz   = 4700.0f;  // 100 Hz-5 kHz
constexpr float kStereoLowHz = 50.0f,  kStereoHighHz = 13200.0f; // 50 Hz-14 kHz

// MTS stereo: L-R subcarrier HF loss ~-6 dB @ 10 kHz (shelf plateau -12 dB;
// an RBJ shelf sits at half its plateau gain at its corner). HIST-APPROX.
constexpr float kSideShelfHz = 10000.0f;
constexpr float kSideShelfDb = -12.0f;

constexpr float kSmoothSeconds = 0.030f;   // block-rate intensity smoothing

inline float softClip (float x) noexcept
{
    const float ax = std::abs (x);
    if (ax <= kClipKnee)
        return x;
    const float y = kClipKnee + kClipSpan * fastTanh ((ax - kClipKnee) / kClipSpan);
    return x < 0.0f ? -y : y;
}
} // namespace

void BroadcastChain::prepare (const StreamSpec& spec)
{
    streamSpec = spec;
    channels.assign ((size_t) spec.numChannels, {});
    for (auto& c : channels)
        c.dc.prepare (spec.sampleRate);
    os.prepare (spec);
    smoothedIntensity = -1.0f;              // snap to target on first block
    cachedIntensity   = -1.0f;
    cachedBand        = -1;                 // force coefficient refresh
}

void BroadcastChain::reset()
{
    for (auto& c : channels)
    {
        c.preEmph.reset(); c.deEmph.reset(); c.ripple1.reset(); c.ripple2.reset();
        c.bandHp.reset(); c.bandLp.reset(); c.dc.reset();
    }
    sideHfLoss.reset();
    os.reset();
}

void BroadcastChain::process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap,
                              float intensity)
{
    const int numSamples = buffer.getNumSamples();
    const int numCh = std::min (buffer.getNumChannels(), (int) channels.size());
    if (numSamples == 0 || numCh == 0)
        return;

    const double sr = streamSpec.sampleRate;
    const int band = snap.medium == Medium::broadcastMono ? 0 : 1;

    // Block-rate intensity smoothing: no zipper on macro moves.
    const float target = std::clamp (intensity, 0.0f, 1.0f);
    if (smoothedIntensity < 0.0f)
        smoothedIntensity = target;
    smoothedIntensity += (1.0f - std::exp (-float (numSamples) / (kSmoothSeconds * float (sr))))
                       * (target - smoothedIntensity);
    const float ii = smoothedIntensity;

    // Coefficients recomputed only on a real change; state carries across.
    if (band != cachedBand || std::abs (ii - cachedIntensity) > 0.004f)
    {
        cachedBand      = band;
        cachedIntensity = ii;
        const float lowHz  = band == 0 ? kMonoLowHz  : kStereoLowHz;
        const float highHz = band == 0 ? kMonoHighHz : kStereoHighHz;
        // Band corners blend toward transparent in log frequency.
        const float lpMax = float (sr) * 0.49f;
        const float hpHz  = 5.0f * std::pow (lowHz / 5.0f, ii);
        const float lpHz  = lpMax * std::pow (highHz / lpMax, ii);
        for (auto& c : channels)
        {
            c.preEmph.highShelf (sr, kEmphHz, 1.0f,  kEmphDb * ii);
            c.deEmph.highShelf  (sr, kEmphHz, 1.0f, -kEmphDb * ii);
            c.ripple1.peak (sr, kRipple1Hz, kRipple1Q, kRipple1Db * ii);
            c.ripple2.peak (sr, kRipple2Hz, kRipple2Q, kRipple2Db * ii);
            c.bandHp.highpass (sr, hpHz, 0.7071f);
            c.bandLp.lowpass  (sr, lpHz, 0.7071f);
        }
        sideHfLoss.highShelf (sr, kSideShelfHz, 1.0f, kSideShelfDb * ii);
    }

    // Transmitter: pre-emphasis into the deviation limiter.
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& c = channels[(size_t) ch];
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            d[i] = c.preEmph.process (d[i]);
    }

    // Over-deviation soft clip, oversampled (eco 1x / standard 2x / high 4x).
    // Dry blend inside the stage keeps intensity 0 transparent through it.
    os.process (buffer, snap.quality,
                [ii] (float* const* chans, int nCh, int n, double)
                {
                    for (int c = 0; c < nCh; ++c)
                    {
                        float* s = chans[c];
                        for (int i = 0; i < n; ++i)
                            s[i] += ii * (softClip (s[i]) - s[i]);
                    }
                });

    // Receiver: de-emphasis, IF ripple, band limit.
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& c = channels[(size_t) ch];
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float x = c.deEmph.process (d[i]);
            x = c.ripple1.process (x);
            x = c.ripple2.process (x);
            x = c.bandHp.process (x);
            d[i] = c.bandLp.process (x);
        }
    }

    // MTS stereo: the L-R subcarrier loses HF relative to L+R (HIST-APPROX).
    if (snap.medium == Medium::broadcastStereo && numCh >= 2)
    {
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float mid  = 0.5f * (l[i] + r[i]);
            const float side = sideHfLoss.process (0.5f * (l[i] - r[i]));
            l[i] = mid + side;
            r[i] = mid - side;
        }
    }

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& c = channels[(size_t) ch];
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            d[i] = c.dc.process (d[i]);
    }
}

} // namespace vfa::dsp
