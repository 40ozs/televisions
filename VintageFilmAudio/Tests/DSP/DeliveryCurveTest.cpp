#include "../Harness/VfaTest.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/DeliveryCurves/DeliveryCurveModule.h"
#include "DSP/DeliveryCurves/CurveTables.h"

using namespace vfa::dsp;

namespace
{
// Measure the module's magnitude response at hz via a long sine burst
// (steady-state, Goertzel), which also exercises the real processing path.
double measureResponseDb (DeliveryCurveModule& mod, const ParamSnapshot& snap,
                          double hz, double sampleRate)
{
    const int n = (int) sampleRate; // 1 s
    juce::AudioBuffer<float> buf (1, n);
    auto* d = buf.getWritePointer (0);
    for (int i = 0; i < n; ++i)
        d[i] = 0.25f * std::sin (2.0 * M_PI * hz * i / sampleRate);

    mod.reset();
    // Process in blocks like a host would.
    for (int pos = 0; pos < n; pos += 512)
    {
        const int len = std::min (512, n - pos);
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 1, pos, len);
        mod.process (view, snap);
    }
    // Analyze the second half (steady state).
    const int half = n / 2;
    return vfatest::amplitudeToDb (vfatest::goertzelAmplitude (d + half, half, hz, sampleRate))
         - vfatest::amplitudeToDb (0.25);
}

ParamSnapshot neutralSnap (CurveMode mode)
{
    ParamSnapshot s;
    s.curveMode = mode;
    // Neutralize advanced delivery controls so the table targets are isolated.
    s.delLfRollHz = 20.0f;
    s.delHfRollHz = 20000.0f;
    s.delMidShapeDb = 0.0f;
    s.academyAmt = 1.0f;
    s.xcurveAmt = 1.0f;
    s.playbackSize = 0.5f;
    return s;
}

const char* curveName (CurveMode m)
{
    switch (m)
    {
        case CurveMode::neutral: return "neutral";
        case CurveMode::academy: return "academy";
        case CurveMode::xCurve: return "xcurve";
        case CurveMode::xCurveSmallRoom: return "xcurve_small";
        case CurveMode::earlyTv: return "early_tv";
        case CurveMode::kinescope: return "kinescope";
        case CurveMode::broadcastMono: return "broadcast_mono";
        case CurveMode::lateTv: return "late_tv";
    }
    return "?";
}

// playbackSize=0.5 is the neutral centre (DEV-005): no allowance needed.
double sizeAllowanceDb (double) { return 0.0; }

void runCurveAssertions (CurveMode mode, double sampleRate)
{
    DeliveryCurveModule mod;
    mod.prepare ({ sampleRate, 512, 1 });
    auto snap = neutralSnap (mode);
    mod.designNow (snap);

    std::vector<std::vector<double>> rows;
    // Reference at 1 kHz: targets are dB re 1 kHz.
    const double ref = measureResponseDb (mod, snap, 1000.0, sampleRate);

    for (const auto& target : curves::testTargetsFor (mode))
    {
        if (target.hz > sampleRate * 0.42) continue;
        const double got = measureResponseDb (mod, snap, target.hz, sampleRate) - ref;
        const double tol = target.toleranceDb + sizeAllowanceDb (target.hz);
        char msg[160];
        std::snprintf (msg, sizeof (msg), "%s %g Hz @ %g: got %.2f dB want %.2f +-%.2f",
                       curveName (mode), target.hz, sampleRate, got, target.dB, tol);
        CHECK_MSG (std::abs (got - target.dB) <= tol, msg);
        rows.push_back ({ (double) target.hz, got, (double) target.dB, tol });
    }
    char fn[128];
    std::snprintf (fn, sizeof (fn), "delivery_%s_%d.csv", curveName (mode), (int) sampleRate);
    vfatest::writeCsv (fn, { "hz", "measured_db", "target_db", "tolerance_db" }, rows);
}
} // namespace

VFA_TEST (DeliveryCurve_targets_44100) { for (int m = 0; m < numCurveModes; ++m) runCurveAssertions ((CurveMode) m, 44100.0); }
VFA_TEST (DeliveryCurve_targets_48000) { for (int m = 0; m < numCurveModes; ++m) runCurveAssertions ((CurveMode) m, 48000.0); }
VFA_TEST (DeliveryCurve_targets_96000) { for (int m = 0; m < numCurveModes; ++m) runCurveAssertions ((CurveMode) m, 96000.0); }

VFA_TEST (DeliveryCurve_neutral_is_transparent)
{
    DeliveryCurveModule mod;
    mod.prepare ({ 48000.0, 512, 1 });
    auto snap = neutralSnap (CurveMode::neutral);
    mod.designNow (snap);
    const double ref = measureResponseDb (mod, snap, 1000.0, 48000.0);
    for (double hz : { 50.0, 100.0, 400.0, 1000.0, 4000.0, 8000.0, 12000.0 })
    {
        const double got = measureResponseDb (mod, snap, hz, 48000.0) - ref;
        CHECK_NEAR (got, 0.0, 0.25 + sizeAllowanceDb (hz));
    }
}

VFA_TEST (DeliveryCurve_stability_impulse_decays)
{
    DeliveryCurveModule mod;
    mod.prepare ({ 48000.0, 512, 1 });
    auto snap = neutralSnap (CurveMode::academy);
    mod.designNow (snap);

    juce::AudioBuffer<float> buf (1, 48000);
    buf.clear();
    buf.getWritePointer (0)[0] = 1.0f;
    for (int pos = 0; pos < 48000; pos += 512)
    {
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 1, pos, std::min (512, 48000 - pos));
        mod.process (view, snap);
    }
    const auto* d = buf.getReadPointer (0);
    CHECK (! vfatest::hasNanOrInf (d, 48000));
    // Energy after 600 taps must be negligible (FIR: exactly zero beyond len).
    CHECK (vfatest::rmsDb (d + 1024, 48000 - 1024) < -120.0);
}

VFA_TEST (DeliveryCurve_group_delay_low_above_100hz)
{
    // Minimum-phase design: group delay above 100 Hz must stay under 1 ms
    // (DSP_SPEC §2). Estimate via phase difference of two close frequencies
    // measured through cross-correlation peak of narrowband bursts — cheaper
    // proxy: impulse response centroid of energy must sit early.
    DeliveryCurveModule mod;
    mod.prepare ({ 48000.0, 512, 1 });
    auto snap = neutralSnap (CurveMode::academy);
    mod.designNow (snap);

    juce::AudioBuffer<float> buf (1, 4096);
    buf.clear();
    buf.getWritePointer (0)[0] = 1.0f;
    for (int pos = 0; pos < 4096; pos += 512)
    {
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 1, pos, 512);
        mod.process (view, snap);
    }
    const auto* d = buf.getReadPointer (0);
    double num = 0, den = 0;
    for (int i = 0; i < 1024; ++i)
    {
        const double e = (double) d[i] * d[i];
        num += e * i; den += e;
    }
    const double centroidMs = (num / std::max (den, 1.0e-12)) / 48.0;
    CHECK_MSG (centroidMs < 1.0, "impulse energy centroid > 1 ms");
}

VFA_TEST (DeliveryCurve_redesign_crossfade_is_smooth)
{
    DeliveryCurveModule mod;
    mod.prepare ({ 48000.0, 512, 1 });
    auto snap = neutralSnap (CurveMode::neutral);
    mod.designNow (snap);

    // Steady sine while switching to academy: no sample-to-sample jump.
    juce::AudioBuffer<float> buf (1, 48000);
    auto* d = buf.getWritePointer (0);
    for (int i = 0; i < 48000; ++i)
        d[i] = 0.25f * std::sin (2.0 * M_PI * 1000.0 * i / 48000.0);

    auto snap2 = neutralSnap (CurveMode::academy);
    bool requested = false;
    for (int pos = 0; pos < 48000; pos += 512)
    {
        if (pos > 4096 && ! requested)
            requested = mod.requestRedesign (snap2);
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 1, pos, std::min (512, 48000 - pos));
        mod.process (view, snap);
    }
    CHECK (requested);
    CHECK (! vfatest::hasNanOrInf (d, 48000));
    double maxStep = 0;
    for (int i = 1; i < 48000; ++i)
        maxStep = std::max (maxStep, (double) std::abs (d[i] - d[i - 1]));
    // 1 kHz sine at 0.25 amplitude has natural step ~0.033; allow filter
    // coloration but no discontinuity.
    CHECK_MSG (maxStep < 0.09, "discontinuity during curve crossfade");
}
