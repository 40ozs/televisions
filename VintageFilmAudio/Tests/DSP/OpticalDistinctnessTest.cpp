#include "../Harness/VfaTest.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/Optical/OpticalModel.h"
#include "DSP/Core/Rng.h"

using namespace vfa::dsp;

namespace
{
constexpr double kSr = 48000.0;

ParamSnapshot opticalSnap()
{
    ParamSnapshot s;
    s.medium = Medium::opticalMono;
    s.quality = Quality::standard;
    s.opticalMode = OpticalMode::variableArea;
    s.opticalDist = 0.4f;
    s.imageSpread = 0.0f;
    return s;
}

// Process a whole buffer in host-sized blocks through a freshly prepared model.
void runModel (juce::AudioBuffer<float>& buf, const ParamSnapshot& snap,
               float intensity = 1.0f, float extraDriveDb = 0.0f)
{
    OpticalModel mod;
    mod.prepare ({ kSr, 512, buf.getNumChannels() });
    const int n = buf.getNumSamples();
    for (int pos = 0; pos < n; pos += 512)
    {
        const int len = std::min (512, n - pos);
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(),
                                       buf.getNumChannels(), pos, len);
        mod.process (view, snap, intensity, extraDriveDb);
    }
}

juce::AudioBuffer<float> makeSine (double hz, float amp, int n)
{
    juce::AudioBuffer<float> buf (1, n);
    auto* d = buf.getWritePointer (0);
    for (int i = 0; i < n; ++i)
        d[i] = amp * (float) std::sin (2.0 * M_PI * hz * i / kSr);
    return buf;
}

// Deterministic pink-ish noise: white (seeded Rng) through a one-pole LP with
// a small white bleed so 6-10 kHz still carries measurable energy.
juce::AudioBuffer<float> makePinkishNoise (float rmsTarget, int n, uint64_t seed)
{
    juce::AudioBuffer<float> buf (1, n);
    auto* d = buf.getWritePointer (0);
    Rng rng (seed);
    OnePoleLP lp;
    lp.setCutoff (1500.0f, kSr);
    for (int i = 0; i < n; ++i)
    {
        const float w = rng.nextBipolar();
        d[i] = lp.process (w) + 0.2f * w;
    }
    const double r = vfatest::rms (d, n);
    buf.applyGain ((float) (rmsTarget / std::max (r, 1.0e-9)));
    return buf;
}

// THD from 2nd + 3rd harmonics only (the 6 kHz tone's higher harmonics sit
// beyond Nyquist; use the same measure for every tone so ratios are fair).
double thd23 (const float* x, int n, double f0)
{
    return vfatest::thdPercent (x, n, f0, kSr, 3);
}

// Plain hard clipper reference: y = clamp(x, -t, t).
void hardClip (juce::AudioBuffer<float>& buf, float t)
{
    auto* d = buf.getWritePointer (0);
    for (int i = 0; i < buf.getNumSamples(); ++i)
        d[i] = std::clamp (d[i], -t, t);
}

// Find the clip threshold that gives (approximately) the requested THD for a
// sine of amplitude amp — clip THD depends only on t/amp, so bisect on it.
float clipThresholdForThd (double targetThdPercent, float amp)
{
    const int n = 4800;   // 200 Hz -> 20 exact cycles
    float lo = 0.05f * amp, hi = 0.999f * amp, t = hi;
    for (int it = 0; it < 40; ++it)
    {
        t = 0.5f * (lo + hi);
        auto buf = makeSine (200.0, amp, n);
        hardClip (buf, t);
        const double thd = thd23 (buf.getReadPointer (0), n, 200.0);
        if (thd > targetThdPercent) lo = t; else hi = t;
    }
    return t;
}

// Ratio (dB) of 6-10 kHz band energy to total energy, over the last part of
// the buffer (offline analysis helper, bandpass biquad at 8 kHz).
double hfToTotalDb (const float* x, int n)
{
    const int skip = 4800;                       // settle envelopes/filters
    static thread_local std::vector<float> bp;
    bp.assign ((size_t) n, 0.0f);
    Biquad band;
    band.bandpass (kSr, 8000.0f, 2.0f);   // ~6-10 kHz
    for (int i = 0; i < n; ++i)
        bp[(size_t) i] = band.process (x[i]);
    return vfatest::rmsDb (bp.data() + skip, n - skip) - vfatest::rmsDb (x + skip, n - skip);
}
} // namespace

// 1. Sibilance-selective distortion: at equal input level the 6 kHz tone must
//    distort far more than the 200 Hz tone (pre-emphasis drives HF into the
//    knee first), unlike a generic clipper which treats both alike.
VFA_TEST (Optical_hf_selective_distortion)
{
    const int n = 48000;
    const float amp = dbToGain (-10.0f);
    auto snap = opticalSnap();
    snap.opticalDist = 0.6f;
    snap.imageSpread = 0.0f;

    auto low = makeSine (200.0, amp, n);
    runModel (low, snap);
    auto high = makeSine (6000.0, amp, n);
    runModel (high, snap);

    const int half = n / 2;
    const double thdLow  = thd23 (low.getReadPointer (0) + half, half, 200.0);
    const double thdHigh = thd23 (high.getReadPointer (0) + half, half, 6000.0);
    char msg[160];
    std::snprintf (msg, sizeof (msg), "THD 6 kHz %.2f%% vs 200 Hz %.2f%% (want >= 3x)",
                   thdHigh, thdLow);
    CHECK_MSG (thdLow > 0.05, "200 Hz tone should show some distortion");
    CHECK_MSG (thdHigh >= 3.0 * thdLow, msg);

    // Reference: a plain hard clipper with matched THD at 200 Hz shows no
    // frequency selectivity at all (ratio ~1).
    const float t = clipThresholdForThd (thdLow, amp);
    auto clipLow = makeSine (200.0, amp, n);
    hardClip (clipLow, t);
    auto clipHigh = makeSine (6000.0, amp, n);
    hardClip (clipHigh, t);
    const double cLow  = thd23 (clipLow.getReadPointer (0) + half, half, 200.0);
    const double cHigh = thd23 (clipHigh.getReadPointer (0) + half, half, 6000.0);
    REQUIRE (cLow > 0.05);
    const double clipRatio = cHigh / cLow;
    std::snprintf (msg, sizeof (msg), "hard clip ratio %.2f (want ~1)", clipRatio);
    CHECK_MSG (clipRatio > 0.5 && clipRatio < 2.0, msg);
}

// 2. Image spread: HF content (relative to total) must DROP as level rises —
//    the opposite of what any level-driven clipper does.
VFA_TEST (Optical_image_spread_level_dependence)
{
    const int n = 48000;
    auto snap = opticalSnap();
    snap.opticalDist = 0.2f;
    snap.imageSpread = 1.0f;

    auto quiet = makePinkishNoise (dbToGain (-30.0f), n, 20260727u);
    runModel (quiet, snap);
    auto loud = makePinkishNoise (dbToGain (-8.0f), n, 20260727u);
    runModel (loud, snap);

    const double hfQuiet = hfToTotalDb (quiet.getReadPointer (0), n);
    const double hfLoud  = hfToTotalDb (loud.getReadPointer (0), n);
    char msg[160];
    std::snprintf (msg, sizeof (msg),
                   "HF/total quiet %.2f dB, loud %.2f dB (want loud lower by >= 2 dB)",
                   hfQuiet, hfLoud);
    CHECK_MSG (hfQuiet - hfLoud >= 2.0, msg);
}

// 3. Transfer-curve parity: variable density is 2nd-harmonic dominant,
//    variable area 3rd-harmonic dominant.
VFA_TEST (Optical_va_vs_vd_harmonic_parity)
{
    const int n = 48000;
    const int half = n / 2;
    const float amp = dbToGain (-12.0f);
    auto snap = opticalSnap();
    snap.opticalDist = 0.6f;

    snap.opticalMode = OpticalMode::variableDensity;
    auto vdBuf = makeSine (1000.0, amp, n);
    runModel (vdBuf, snap);
    const double vd2 = vfatest::goertzelAmplitude (vdBuf.getReadPointer (0) + half, half, 2000.0, kSr);
    const double vd3 = vfatest::goertzelAmplitude (vdBuf.getReadPointer (0) + half, half, 3000.0, kSr);
    char msg[160];
    std::snprintf (msg, sizeof (msg), "VD h2 %.1f dB, h3 %.1f dB",
                   vfatest::amplitudeToDb (vd2), vfatest::amplitudeToDb (vd3));
    CHECK_MSG (vd2 > vd3, msg);

    snap.opticalMode = OpticalMode::variableArea;
    auto vaBuf = makeSine (1000.0, amp, n);
    runModel (vaBuf, snap);
    const double va2 = vfatest::goertzelAmplitude (vaBuf.getReadPointer (0) + half, half, 2000.0, kSr);
    const double va3 = vfatest::goertzelAmplitude (vaBuf.getReadPointer (0) + half, half, 3000.0, kSr);
    std::snprintf (msg, sizeof (msg), "VA h2 %.1f dB, h3 %.1f dB",
                   vfatest::amplitudeToDb (va2), vfatest::amplitudeToDb (va3));
    CHECK_MSG (va3 > va2, msg);
}

// 4. The asymmetric VD curve generates DC; the servo must remove it.
VFA_TEST (Optical_dc_removed)
{
    const int n = 72000;   // 1.5 s
    auto snap = opticalSnap();
    snap.opticalMode = OpticalMode::variableDensity;
    snap.opticalDist = 0.6f;
    auto buf = makeSine (1000.0, dbToGain (-8.0f), n);
    runModel (buf, snap);

    const int tail = 24000;   // last 0.5 s
    const auto* d = buf.getReadPointer (0) + (n - tail);
    double mean = 0.0;
    for (int i = 0; i < tail; ++i)
        mean += d[i];
    mean /= tail;
    char msg[128];
    std::snprintf (msg, sizeof (msg), "residual DC %.6f (want |DC| < 0.001)", mean);
    CHECK_MSG (std::abs (mean) < 0.001, msg);
}

// 5. intensity = 0 must be fully transparent (blend), band limit included.
VFA_TEST (Optical_intensity_zero_transparent)
{
    const int n = 24000;
    const float amp = dbToGain (-18.0f);
    auto snap = opticalSnap();
    auto buf = makeSine (1000.0, amp, n);
    runModel (buf, snap, 0.0f);

    const int half = n / 2;
    const double out = vfatest::goertzelAmplitude (buf.getReadPointer (0) + half, half, 1000.0, kSr);
    const double errDb = vfatest::amplitudeToDb (out) - vfatest::amplitudeToDb (amp);
    CHECK_NEAR (errDb, 0.0, 0.1);
}

// 6. Oversampling keeps aliasing of a 15 kHz tone's harmonics bounded:
//    2*15k = 30k folds to 18k, 3*15k = 45k folds to 3k.
VFA_TEST (Optical_aliasing_bounded)
{
    const int n = 48000;
    const int half = n / 2;
    const float amp = dbToGain (-12.0f);
    auto snap = opticalSnap();
    snap.opticalDist = 0.7f;
    snap.imageSpread = 0.0f;

    auto aliasRelDb = [&] (Quality q)
    {
        auto s = snap;
        s.quality = q;
        auto buf = makeSine (15000.0, amp, n);
        runModel (buf, s);
        const auto* d = buf.getReadPointer (0) + half;
        const double fund = vfatest::goertzelAmplitude (d, half, 15000.0, kSr);
        const double a18k = vfatest::goertzelAmplitude (d, half, 18000.0, kSr);
        const double a3k  = vfatest::goertzelAmplitude (d, half, 3000.0, kSr);
        return vfatest::amplitudeToDb (std::max (a18k, a3k)) - vfatest::amplitudeToDb (fund);
    };

    const double stdRel = aliasRelDb (Quality::standard);
    const double ecoRel = aliasRelDb (Quality::eco);
    char msg[160];
    std::snprintf (msg, sizeof (msg), "standard alias %.1f dB rel fundamental", stdRel);
    CHECK_MSG (stdRel < -40.0, msg);
    std::snprintf (msg, sizeof (msg), "eco alias %.1f dB vs standard %.1f dB", ecoRel, stdRel);
    CHECK_MSG (stdRel <= ecoRel + 1.0, msg);
}

// 7. Stability under abuse: full-scale square wave at extreme settings,
//    stereo, 4x oversampling, including empty and single-sample blocks.
VFA_TEST (Optical_no_nan_and_stability)
{
    const int n = 96000;   // 2 s
    juce::AudioBuffer<float> buf (2, n);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            d[i] = (i / 200) % 2 == 0 ? 1.0f : -1.0f;   // 120 Hz square
    }
    auto snap = opticalSnap();
    snap.quality = Quality::high;
    snap.opticalDist = 1.0f;
    snap.imageSpread = 1.0f;
    snap.opticalMode = OpticalMode::variableDensity;

    OpticalModel mod;
    mod.prepare ({ kSr, 512, 2 });
    int pos = 0;
    int step = 1;   // ragged block sizes: 1, 2, 3, ... then 512s
    while (pos < n)
    {
        const int len = std::min (step < 512 ? step++ : 512, n - pos);
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, pos, len);
        mod.process (view, snap, 1.0f, 6.0f);
        if (pos == 0)
        {
            juce::AudioBuffer<float> empty (buf.getArrayOfWritePointers(), 2, 0, 0);
            mod.process (empty, snap, 1.0f, 6.0f);   // zero-length must be safe
        }
        pos += len;
    }

    for (int ch = 0; ch < 2; ++ch)
    {
        const auto* d = buf.getReadPointer (ch);
        CHECK (! vfatest::hasNanOrInf (d, n));
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
            peak = std::max (peak, std::abs (d[i]));
        char msg[96];
        std::snprintf (msg, sizeof (msg), "peak %.3f (want < 4)", peak);
        CHECK_MSG (peak < 4.0f, msg);
    }
}
