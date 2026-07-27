#include "OpticalModel.h"

namespace vfa::dsp
{

namespace
{
// ---------------------------------------------------------------- constants
// Reference level: -18 dBFS = 0 VU nominal (DSP_SPEC preamble).
constexpr float kNominal = 0.12589254f;

// Slit-loss LPF (SPEC 4.1): Gaussian-ish rolloff from two cascaded low-Q
// biquads, each -1.5 dB at the published corner so the cascade sits at -3 dB
// there and tracks a near-Gaussian curve well past it (HIST-APPROX: scanning
// slit MTF approximated by filter fit, not derived from slit geometry).
constexpr float kSlitQ       = 0.577f;
constexpr float kSlitFcRatio = 1.783f;   // per-biquad fc = corner * ratio

// HF pre/de-emphasis around the shaper (SPEC 4.2): +/-10 dB shelf @ 4 kHz.
// The RBJ shelf pair at +g / -g dB is an exact magnitude inverse, so the
// emphasis is transparent when the shaper is linear. Slope > 1 keeps the
// transition tight so the sibilance band takes the full boost (HIST-APPROX).
constexpr float kEmphHz    = 4000.0f;
constexpr float kEmphDb    = 10.0f;
constexpr float kEmphSlope = 2.0f;

// Drive mapping (SPEC 4.3): shaper is y = fastTanh(g*x)/g, i.e. unity
// small-signal gain with saturation ceiling 1/g. Calibrated so a -18 dBFS
// 1 kHz sine at opticalDist = 0.4 lands at ~2 % THD and +8 dB over nominal is
// past the ceiling (heavily saturated). Ceiling is capped so it never drops
// below nominal (-18 dBFS), even with generation-loss extra drive.
constexpr float kDriveFloorDb   = 7.0f;
constexpr float kDriveRangeDb   = 9.0f;
constexpr float kDriveMaxDb     = 18.0f;
constexpr float kStereoMilderDb = -3.0f;  // '77-era stereo tracks, milder (SPEC 3)

// Variable-density asymmetry: 2nd-harmonic term applied to the *bounded*
// sigmoid output, y = (t + k*t^2)/g with t = fastTanh(g*x). Monotonic for
// k < 0.5 and bounded by (1 + k)/g; the DC it generates is the servo's job.
constexpr float kVdAsym = 0.42f;

// Modulation-peak rounding (SPEC 4.4, HIST-APPROX "ground-noise-reduction
// shutter" style): fast feedback gain rider (0.3 ms attack / 30 ms release)
// that only compresses the top ~6 dB above nominal — threshold sits
// +2 dB over nominal, slope 0.7 dB/dB (ratio ~3.3:1), correction capped 6 dB.
constexpr float kRoundThresh = kNominal * 1.2589f;  // nominal + 2 dB
constexpr float kRoundSlope  = 0.7f;
constexpr float kRoundCapDb  = 6.0f;

// Control smoothing time constant for gains that can jump.
constexpr float kSmoothSec = 0.02f;

// Slit-loss corner per medium (SPEC 3 bandwidth table, HIST-APPROX).
float slitCornerFor (Medium m) noexcept
{
    // Research doc §1.2: optical tracks carried HF to ~12.5 kHz (~13 kHz
    // with noise reduction); the famous darkness came from the Academy
    // *delivery* curve masking optical hiss, not from the medium itself.
    switch (m)
    {
        case Medium::opticalStereo: return 13000.0f;  // Dolby-era, with NR
        case Medium::kinescope:     return 8000.0f;   // optical print off monitor
        case Medium::opticalMono:   return 12500.0f;
        default:                    return 12000.0f;  // generic optical fallback
    }
}

void copyCoefficients (Biquad& dst, const Biquad& src) noexcept
{
    // Coefficients only; z1/z2 kept so redesigns don't click.
    dst.b0 = src.b0; dst.b1 = src.b1; dst.b2 = src.b2;
    dst.a1 = src.a1; dst.a2 = src.a2;
}

// ------------------------------------------------------------- image spread
// Level-dependent HF loss (SPEC 4.5, HIST-APPROX): envelope-driven one-pole
// whose cutoff drops as level rises — the printed image of a loud trace
// spreads and reads back dull. At full modulation (>= ~-10 dBFS envelope) the
// loss reaches -6 dB @ 8 kHz (one-pole cutoff 8 kHz / sqrt(3) ~= 4.62 kHz);
// at quiet levels the pole parks high and the stage is blended out entirely.
// The distinctness hook: generic clippers *add* HF with level, this removes it.
class LevelImageSpread : public IImageSpreadStage
{
public:
    void prepare (const StreamSpec& spec) override
    {
        sr = spec.sampleRate;
        ctrlCoeff = 1.0f - std::exp (-1.0f / (0.010f * (float) sr));  // ~10 ms
        st.assign ((size_t) spec.numChannels, {});
        for (auto& s : st)
        {
            s.env.prepare (sr, 8.0f, 120.0f);   // RMS-ish program tracker
            s.lp.setCutoff (kFcQuiet, sr);
        }
    }

    void reset() override
    {
        for (auto& s : st) { s.env.reset(); s.lp.reset(); s.ctrl = 0.0f; }
    }

    void process (float* samples, int numSamples, int channel, float amount) override
    {
        if (samples == nullptr || channel < 0 || channel >= (int) st.size())
            return;
        auto& s = st[(size_t) channel];
        amount = std::clamp (amount, 0.0f, 1.0f);
        const float mix = std::min (1.0f, amount * 8.0f);  // exact bypass at 0
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = samples[i];
            const float e = s.env.process (x);
            const float depth = std::clamp ((e - kEnvLo) * kEnvNorm, 0.0f, 1.0f) * amount;
            s.ctrl += ctrlCoeff * (depth - s.ctrl);        // smoothed: no zipper
            s.ctrl = flushToZero (s.ctrl);
            s.lp.setCutoff (kFcQuiet * std::exp (kFcLogSpan * s.ctrl), sr);
            const float lp = s.lp.process (x);
            samples[i] = x + mix * (lp - x);
        }
    }

private:
    // Level window mapped linearly in amplitude, -28 dBFS -> 0, -18 dBFS -> 1.
    // The stage sits after the peak rounding (which trims up to 6 dB off loud
    // program) and uses an RMS-ish tracker, so a >= ~-10 dBFS *input* still
    // reaches full spread depth while quiet passages stay untouched
    // (HIST-APPROX of exposure vs image-spread behaviour).
    static constexpr float kEnvLo     = 0.04f;
    static constexpr float kEnvNorm   = 1.0f / (0.12f - 0.04f);
    static constexpr float kFcQuiet   = 19000.0f;
    static constexpr float kFcLogSpan = -1.4149f;   // ln(4619 / 19000)

    struct Ch { EnvelopeFollower env; OnePoleLP lp; float ctrl = 0.0f; };
    double sr = 48000.0;
    float ctrlCoeff = 0.1f;
    std::vector<Ch> st;
};
} // namespace

OpticalModel::OpticalModel() : imageSpread (std::make_unique<LevelImageSpread>()) {}
OpticalModel::~OpticalModel() = default;

void OpticalModel::prepare (const StreamSpec& spec)
{
    streamSpec = spec;
    channels.assign ((size_t) spec.numChannels, {});
    for (auto& c : channels)
    {
        c.peakEnv.prepare (spec.sampleRate, 0.3f, 30.0f);  // SPEC 4.4 rider times
        c.dc.prepare (spec.sampleRate);                    // 5 Hz servo (SPEC 11)
    }
    imageSpread->prepare (spec);
    os.prepare (spec);
    scratch.setSize (spec.numChannels, std::max (spec.maxBlockSize, 1), false, false, true);

    slitDesignedHz = -1.0f;      // lazily designed from the first snapshot
    emphDesignedDb = -1.0e9f;
    driveGain = 1.0e-4f;
    wetMix = intens = 0.0f;
    reset();
}

void OpticalModel::reset()
{
    for (auto& c : channels)
    {
        c.slit1.reset(); c.slit2.reset();
        c.preEmph.reset(); c.deEmph.reset();
        c.peakEnv.reset(); c.dc.reset();
        c.roundGain = 1.0f;
        c.dryRing.fill (0.0f);
        c.dryPos = 0;
    }
    imageSpread->reset();
    os.reset();
}

void OpticalModel::designSlit (float cornerHz)
{
    Biquad proto;
    proto.lowpass (streamSpec.sampleRate, cornerHz * kSlitFcRatio, kSlitQ);
    for (auto& c : channels)
    {
        copyCoefficients (c.slit1, proto);
        copyCoefficients (c.slit2, proto);
    }
    slitDesignedHz = cornerHz;
}

void OpticalModel::designEmphasis (float shelfDb)
{
    Biquad pre, de;
    pre.highShelf (streamSpec.sampleRate, kEmphHz, kEmphSlope, shelfDb);
    de.highShelf (streamSpec.sampleRate, kEmphHz, kEmphSlope, -shelfDb);
    for (auto& c : channels)
    {
        copyCoefficients (c.preEmph, pre);
        copyCoefficients (c.deEmph, de);
    }
    emphDesignedDb = shelfDb;
}

void OpticalModel::process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap,
                            float intensity, float extraDriveDb)
{
    const int n  = buffer.getNumSamples();
    const int nc = std::min ({ buffer.getNumChannels(), (int) channels.size(),
                               scratch.getNumChannels() });
    if (n <= 0 || nc <= 0 || n > scratch.getNumSamples())
        return;

    const double sr = streamSpec.sampleRate;
    intensity = std::clamp (intensity, 0.0f, 1.0f);

    // Per-block control smoothing (~20 ms) of everything that can jump.
    const float aBlk = 1.0f - std::exp (-(float) n / ((float) sr * kSmoothSec));
    intens += aBlk * (intensity - intens);
    intens = flushToZero (intens);

    // Slit corner per medium; redesign only on (preset-level) change.
    const float corner = slitCornerFor (snap.medium);
    if (std::abs (corner - slitDesignedHz) > 1.0f)
        designSlit (corner);

    // Emphasis depth scales toward transparent with intensity.
    const float emphTarget = kEmphDb * intens;
    if (std::abs (emphTarget - emphDesignedDb) > 0.05f)
        designEmphasis (emphTarget);

    // Drive from opticalDist + generation extra, linearly scaled by intensity
    // (g -> 0 makes the shaper exactly transparent).
    float driveDb = kDriveFloorDb + kDriveRangeDb * std::clamp (snap.opticalDist, 0.0f, 1.0f)
                  + extraDriveDb;
    if (snap.medium == Medium::opticalStereo)
        driveDb += kStereoMilderDb;
    driveDb = std::min (driveDb, kDriveMaxDb);
    const float gTarget = std::max (dbToGain (driveDb) * intensity, 1.0e-4f);
    const float g0 = driveGain;
    driveGain += aBlk * (gTarget - driveGain);

    const float wm0 = wetMix;
    wetMix += aBlk * (std::min (1.0f, intensity * 4.0f) - wetMix);
    wetMix = flushToZero (wetMix);

    // Keep an unprocessed copy for the intensity blend.
    for (int ch = 0; ch < nc; ++ch)
        scratch.copyFrom (ch, 0, buffer, ch, 0, n);

    // ---- slit loss -> modulation-peak rounding -> pre-emphasis (base rate)
    const float invThresh = 1.0f / kRoundThresh;
    for (int ch = 0; ch < nc; ++ch)
    {
        auto& cs = channels[(size_t) ch];
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            float x = cs.slit2.process (cs.slit1.process (d[i]));

            // Feedback gain rider: detector listens to its own output; the
            // 0.3 ms / 30 ms envelope keeps the gain move click-free.
            const float env = cs.peakEnv.process (x * cs.roundGain);
            if (env > kRoundThresh)
            {
                const float grDb = std::min (kRoundCapDb,
                                             kRoundSlope * gainToDb (env * invThresh));
                cs.roundGain = dbToGain (-grDb * intens);
            }
            else
            {
                cs.roundGain = 1.0f;
            }
            x *= cs.roundGain;
            d[i] = cs.preEmph.process (x);
        }
    }

    // ---- nonlinear transfer curve, oversampled (SPEC 4.3 / ADR-009)
    juce::AudioBuffer<float> wet (buffer.getArrayOfWritePointers(), nc, 0, n);
    const bool vd = snap.opticalMode == OpticalMode::variableDensity;
    const float gEnd = driveGain;
    os.process (wet, snap.quality,
                [g0, gEnd, vd] (float* const* chans, int numCh, int num, double) noexcept
    {
        const float dg = num > 0 ? (gEnd - g0) / (float) num : 0.0f;
        for (int c = 0; c < numCh; ++c)
        {
            float* s = chans[c];
            if (s == nullptr)
                continue;
            float g = g0;
            for (int i = 0; i < num; ++i)
            {
                g += dg;                       // per-sample drive ramp
                const float inv = 1.0f / g;    // g >= 1e-4 by construction
                const float t = fastTanh (g * s[i]);
                if (vd)
                    s[i] = (t + kVdAsym * t * t) * inv;  // 2nd-harmonic dominant
                else
                    s[i] = t * inv;                      // odd-harmonic dominant
            }
        }
    });

    // ---- de-emphasis -> image spread -> DC servo -> intensity blend
    const float spreadAmt = std::clamp (snap.imageSpread, 0.0f, 1.0f) * intens;
    const float lat = std::clamp (os.latencySamples (snap.quality), 0.0f, 48.0f);
    const int   di  = (int) lat;
    const float fr  = lat - (float) di;
    static_assert (std::tuple_size<decltype (ChannelState::dryRing)>::value == 64);

    for (int ch = 0; ch < nc; ++ch)
    {
        auto& cs = channels[(size_t) ch];
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            d[i] = cs.deEmph.process (d[i]);

        imageSpread->process (d, n, ch, spreadAmt);

        // Dry path is delay-matched to the oversampler latency so the blend
        // never combs; at intensity 0 the output is the (delayed) input.
        const float* dry = scratch.getReadPointer (ch);
        const float dwm = (wetMix - wm0) / (float) n;
        float wm = wm0;
        for (int i = 0; i < n; ++i)
        {
            cs.dryRing[(size_t) cs.dryPos] = dry[i];
            const int p0 = (cs.dryPos - di) & 63;
            const int p1 = (p0 - 1) & 63;
            cs.dryPos = (cs.dryPos + 1) & 63;
            const float d0 = cs.dryRing[(size_t) p0];
            const float dryDel = d0 + fr * (cs.dryRing[(size_t) p1] - d0);
            const float wetS = cs.dc.process (d[i]);
            wm += dwm;
            d[i] = dryDel + wm * (wetS - dryDel);
        }
    }
}

} // namespace vfa::dsp
