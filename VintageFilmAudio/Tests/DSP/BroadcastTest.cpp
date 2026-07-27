// Broadcast-chain tests: PeriodDynamics (AGC / dialogue focus / compressor /
// limiter, DSP_SPEC §7) and Reproduction (width / mono fold / small speaker).

#include "../Harness/VfaTest.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/Dynamics/PeriodDynamics.h"
#include "DSP/Reproduction/Reproduction.h"

using namespace vfa::dsp;

namespace
{
constexpr double kSr = 48000.0;
constexpr int kBlock = 512;

// All dynamics stages off; individual tests enable exactly what they probe.
ParamSnapshot dynSnap()
{
    ParamSnapshot s;
    s.agc = s.comp = s.limit = s.dialog = s.pump = 0.0f;
    s.relChar = 0.5f;
    return s;
}

// Neutral reproduction: full width, stereo, speaker off.
ParamSnapshot reproSnap()
{
    ParamSnapshot s;
    s.width = 1.0f;
    s.smallSpeaker = 0.0f;
    s.monoMode = MonoMode::stereo;
    s.monoLaw = MonoLaw::minus3dB;
    return s;
}

template <typename Module>
void processBlocks (Module& m, juce::AudioBuffer<float>& buf, const ParamSnapshot& snap)
{
    const int n = buf.getNumSamples();
    for (int pos = 0; pos < n; pos += kBlock)
    {
        const int len = std::min (kBlock, n - pos);
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(),
                                       buf.getNumChannels(), pos, len);
        m.process (view, snap);
    }
}

void fillSine (juce::AudioBuffer<float>& buf, double hz, float amp, float rightScale = 1.0f)
{
    const int n = buf.getNumSamples();
    for (int c = 0; c < buf.getNumChannels(); ++c)
    {
        auto* d = buf.getWritePointer (c);
        const float scale = (c == 1 ? rightScale : 1.0f) * amp;
        for (int i = 0; i < n; ++i)
            d[i] = scale * (float) std::sin (2.0 * M_PI * hz * i / kSr);
    }
}

double lastRmsDb (const juce::AudioBuffer<float>& buf, int ch, double seconds)
{
    const int n = buf.getNumSamples();
    const int len = std::min (n, (int) (seconds * kSr));
    return vfatest::rmsDb (buf.getReadPointer (ch) + (n - len), len);
}

// Sine RMS in dB for a peak level given in dB.
double sineRmsDb (double peakDb) { return peakDb - 3.0103; }

// Steady-state magnitude response of a module at hz (small-signal), via a
// 1 s sine and Goertzel on the second half; module is reset first.
template <typename Module>
double responseDb (Module& m, const ParamSnapshot& snap, double hz, int numCh)
{
    constexpr float amp = 0.05f;
    juce::AudioBuffer<float> buf (numCh, (int) kSr);
    fillSine (buf, hz, amp);
    m.reset();
    processBlocks (m, buf, snap);
    const int half = (int) kSr / 2;
    return vfatest::amplitudeToDb (vfatest::goertzelAmplitude (
               buf.getReadPointer (0) + half, half, hz, kSr))
         - vfatest::amplitudeToDb (amp);
}
} // namespace

// ---------------------------------------------------------------- dynamics

VFA_TEST (Dyn_agc_bounded_and_rides)
{
    auto snap = dynSnap();
    snap.agc = 1.0f;

    // Quiet program is lifted toward -18 dBFS, bounded by +12 dB.
    {
        PeriodDynamics dyn;
        dyn.prepare ({ kSr, kBlock, 1 });
        juce::AudioBuffer<float> buf (1, (int) (4.0 * kSr));
        fillSine (buf, 1000.0, dbToGain (-30.0f));
        processBlocks (dyn, buf, snap);
        const double lift = lastRmsDb (buf, 0, 1.0) - sineRmsDb (-30.0);
        CHECK_MSG (lift >= 4.0 && lift <= 12.0, "AGC lift out of range");
    }
    // Loud program is cut, bounded by -12 dB.
    {
        PeriodDynamics dyn;
        dyn.prepare ({ kSr, kBlock, 1 });
        juce::AudioBuffer<float> buf (1, (int) (3.0 * kSr));
        fillSine (buf, 1000.0, dbToGain (-6.0f));
        processBlocks (dyn, buf, snap);
        const double cut = sineRmsDb (-6.0) - lastRmsDb (buf, 0, 1.0);
        CHECK_MSG (cut >= 2.0 && cut <= 12.0, "AGC cut out of range");
    }
    // Below the -45 dBFS gate the rider freezes: no noise suck-up.
    {
        PeriodDynamics dyn;
        dyn.prepare ({ kSr, kBlock, 1 });
        juce::AudioBuffer<float> buf (1, (int) (2.0 * kSr));
        fillSine (buf, 1000.0, dbToGain (-50.0f));
        processBlocks (dyn, buf, snap);
        const double change = lastRmsDb (buf, 0, 1.0) - sineRmsDb (-50.0);
        CHECK_MSG (std::abs (change) < 3.0, "gated signal moved");
    }
}

VFA_TEST (Dyn_agc_gain_bounded_always)
{
    auto snap = dynSnap();
    snap.agc = 1.0f;

    // Alternating loud/quiet program: inferred gain must stay within
    // +-12.5 dB at all times (rider bound is +-12 dB).
    const int n = (int) (6.0 * kSr);
    const int seg = (int) (0.5 * kSr);
    juce::AudioBuffer<float> buf (1, n);
    std::vector<float> input ((size_t) n);
    auto* d = buf.getWritePointer (0);
    for (int i = 0; i < n; ++i)
    {
        const float amp = ((i / seg) % 2 == 0) ? dbToGain (-6.0f) : dbToGain (-30.0f);
        d[i] = amp * (float) std::sin (2.0 * M_PI * 1000.0 * i / kSr);
        input[(size_t) i] = d[i];
    }
    PeriodDynamics dyn;
    dyn.prepare ({ kSr, kBlock, 1 });
    processBlocks (dyn, buf, snap);

    const int win = (int) (0.1 * kSr);
    for (int pos = 0; pos + win <= n; pos += win)
    {
        const double inDb  = vfatest::rmsDb (input.data() + pos, win);
        const double outDb = vfatest::rmsDb (buf.getReadPointer (0) + pos, win);
        if (inDb > -60.0)
        {
            char msg[96];
            std::snprintf (msg, sizeof (msg), "window @%d: gain %.2f dB", pos, outDb - inDb);
            CHECK_MSG (std::abs (outDb - inDb) <= 12.5, msg);
        }
    }
}

VFA_TEST (Dyn_compressor_ratio_and_release_character)
{
    // Ratio: at comp = 0.8 an 18 dB input step compresses to <= 13 dB.
    auto steadyOutDb = [] (float peakDb)
    {
        auto snap = dynSnap();
        snap.comp = 0.8f;
        PeriodDynamics dyn;
        dyn.prepare ({ kSr, kBlock, 1 });
        juce::AudioBuffer<float> buf (1, (int) (2.0 * kSr));
        fillSine (buf, 1000.0, dbToGain (peakDb));
        processBlocks (dyn, buf, snap);
        return lastRmsDb (buf, 0, 0.5);
    };
    const double outDelta = steadyOutDb (-6.0f) - steadyOutDb (-24.0f);
    CHECK_MSG (outDelta <= 13.0, "not enough compression at comp=0.8");
    CHECK_MSG (outDelta > 2.0, "implausible output delta");

    // Release character: after a -6 dBFS burst into a -30 dBFS bed, vari-mu
    // (relChar 0) must recover to steady state slower than FET (relChar 1).
    auto recoverySec = [] (float relChar)
    {
        auto snap = dynSnap();
        snap.comp = 0.8f;
        snap.relChar = relChar;
        const int n = (int) (5.0 * kSr);
        const int burst = (int) kSr;                      // 1 s burst, 4 s bed
        juce::AudioBuffer<float> buf (1, n);
        auto* d = buf.getWritePointer (0);
        for (int i = 0; i < n; ++i)
        {
            const float amp = i < burst ? dbToGain (-6.0f) : dbToGain (-30.0f);
            d[i] = amp * (float) std::sin (2.0 * M_PI * 1000.0 * i / kSr);
        }
        PeriodDynamics dyn;
        dyn.prepare ({ kSr, kBlock, 1 });
        processBlocks (dyn, buf, snap);

        const double steady = lastRmsDb (buf, 0, 0.5);
        const int win = (int) (0.05 * kSr);
        for (int pos = burst; pos + win <= n; pos += win)
            if (std::abs (vfatest::rmsDb (buf.getReadPointer (0) + pos, win) - steady) <= 1.0)
                return (pos - burst) / kSr;
        return 4.0;   // never recovered inside the bed
    };
    const double slow = recoverySec (0.0f);
    const double fast = recoverySec (1.0f);
    char msg[96];
    std::snprintf (msg, sizeof (msg), "recovery vari-mu %.2fs vs FET %.2fs", slow, fast);
    CHECK_MSG (slow > fast + 0.3, msg);
    CHECK_MSG (fast < 1.0, "FET recovery too slow");
    CHECK_MSG (slow < 3.9, "vari-mu never recovered");
}

VFA_TEST (Dyn_limiter_ceiling)
{
    auto snap = dynSnap();
    snap.limit = 1.0f;   // threshold -14 dBFS

    PeriodDynamics dyn;
    dyn.prepare ({ kSr, kBlock, 1 });
    juce::AudioBuffer<float> buf (1, (int) (1.5 * kSr));
    fillSine (buf, 1000.0, 1.0f);   // 0 dBFS
    processBlocks (dyn, buf, snap);

    const auto* d = buf.getReadPointer (0);
    const int n = buf.getNumSamples();
    CHECK (! vfatest::hasNanOrInf (d, n));
    float peak = 0.0f;
    for (int i = n - (int) (0.5 * kSr); i < n; ++i)
        peak = std::max (peak, std::abs (d[i]));
    CHECK_MSG (peak <= dbToGain (-11.0f), "limiter ceiling exceeded");
}

VFA_TEST (Dyn_dialog_focus_shape)
{
    auto snap = dynSnap();
    snap.dialog = 1.0f;
    PeriodDynamics dyn;
    dyn.prepare ({ kSr, kBlock, 1 });

    const double r60   = responseDb (dyn, snap, 60.0, 1);
    const double r150  = responseDb (dyn, snap, 150.0, 1);
    const double r400  = responseDb (dyn, snap, 400.0, 1);
    const double r1k   = responseDb (dyn, snap, 1000.0, 1);
    const double r2k   = responseDb (dyn, snap, 2000.0, 1);
    const double r6k   = responseDb (dyn, snap, 6000.0, 1);

    CHECK_MSG (r2k - r400 >= 2.0, "presence peak missing");
    CHECK_MSG (r60 - r1k <= -2.0, "low cut missing");
    // Intelligibility tilt, not a telephone band-pass:
    CHECK_MSG (r150 - r1k > -8.0, "150 Hz killed (telephone!)");
    CHECK_MSG (r6k - r1k > -4.0, "6 kHz killed (telephone!)");
}

// ------------------------------------------------------------ reproduction

VFA_TEST (Repro_mono_fold_laws)
{
    // Correlated L == R at -18 dBFS: the -6 dB law reproduces the input
    // level exactly ((L+R)*0.5 == L); the -3 dB law is +3 dB hotter.
    auto foldOutDb = [] (MonoLaw law)
    {
        auto snap = reproSnap();
        snap.monoMode = MonoMode::mono;
        snap.monoLaw = law;
        Reproduction rep;
        rep.prepare ({ kSr, kBlock, 2 });
        juce::AudioBuffer<float> buf (2, (int) kSr);
        fillSine (buf, 1000.0, dbToGain (-18.0f));
        processBlocks (rep, buf, snap);
        return lastRmsDb (buf, 0, 0.5);
    };
    const double out6 = foldOutDb (MonoLaw::minus6dB);
    const double out3 = foldOutDb (MonoLaw::minus3dB);
    CHECK_NEAR (out6, sineRmsDb (-18.0), 0.35);
    CHECK_NEAR (out3 - out6, 3.01, 0.35);

    // Uncorrelated noise, -3 dB law: output RMS ~= input channel RMS.
    {
        auto snap = reproSnap();
        snap.monoMode = MonoMode::mono;
        snap.monoLaw = MonoLaw::minus3dB;
        Reproduction rep;
        rep.prepare ({ kSr, kBlock, 2 });
        const int n = (int) (2.0 * kSr);
        juce::AudioBuffer<float> buf (2, n);
        uint32_t rng = 12345u;
        for (int c = 0; c < 2; ++c)
        {
            auto* d = buf.getWritePointer (c);
            for (int i = 0; i < n; ++i)
            {
                rng = rng * 1664525u + 1013904223u;   // LCG, deterministic
                d[i] = 0.1f * (2.0f * (float) rng / 4294967296.0f - 1.0f);
            }
        }
        const double inDb = vfatest::rmsDb (buf.getReadPointer (0), n);
        processBlocks (rep, buf, snap);
        const double outDb = vfatest::rmsDb (buf.getReadPointer (0), n);
        CHECK_NEAR (outDb, inDb, 1.5);
    }
}

VFA_TEST (Repro_width_and_narrow)
{
    // Anti-phase program (pure side): width 0 collapses it to silence.
    {
        auto snap = reproSnap();
        snap.width = 0.0f;
        Reproduction rep;
        rep.prepare ({ kSr, kBlock, 2 });
        juce::AudioBuffer<float> buf (2, (int) kSr);
        fillSine (buf, 1000.0, 0.1f, -1.0f);   // L = -R
        processBlocks (rep, buf, snap);
        CHECK_MSG (lastRmsDb (buf, 0, 0.5) < -40.0, "width 0 leaks side");
    }
    // Narrow mode: side reduced by 8-12 dB (nominal 0.35 => -9.1 dB).
    {
        auto snap = reproSnap();
        snap.monoMode = MonoMode::narrow;
        Reproduction rep;
        rep.prepare ({ kSr, kBlock, 2 });
        juce::AudioBuffer<float> buf (2, (int) kSr);
        fillSine (buf, 1000.0, 0.1f, -1.0f);
        const double inDb = sineRmsDb (vfatest::amplitudeToDb (0.1));
        processBlocks (rep, buf, snap);
        const double drop = inDb - lastRmsDb (buf, 0, 0.5);
        CHECK_MSG (drop >= 8.0 && drop <= 12.0, "narrow side reduction out of range");
    }
}

VFA_TEST (Repro_speaker_not_telephone)
{
    Reproduction rep;
    rep.prepare ({ kSr, kBlock, 1 });

    // Half strength must keep speech band edges alive.
    auto snap = reproSnap();
    snap.smallSpeaker = 0.5f;
    {
        const double r1k  = responseDb (rep, snap, 1000.0, 1);
        const double r150 = responseDb (rep, snap, 150.0, 1);
        const double r6k  = responseDb (rep, snap, 6000.0, 1);
        CHECK_MSG (r150 - r1k >= -8.0, "150 Hz killed at amount 0.5");
        CHECK_MSG (r6k - r1k >= -6.0, "6 kHz killed at amount 0.5");
    }
    // Full strength: band limiting IS happening, and the cabinet resonance
    // is visible somewhere in 150-400 Hz.
    snap.smallSpeaker = 1.0f;
    {
        const double r1k = responseDb (rep, snap, 1000.0, 1);
        const double r60 = responseDb (rep, snap, 60.0, 1);
        CHECK_MSG (r60 - r1k <= -10.0, "no LF band limiting at amount 1");
        double best = -100.0;
        for (double hz : { 200.0, 250.0, 280.0, 300.0, 330.0, 360.0, 400.0 })
            best = std::max (best, responseDb (rep, snap, hz, 1) - r1k);
        CHECK_MSG (best >= 1.5, "cabinet resonance not visible");
    }
}

// ------------------------------------------------------------- robustness

VFA_TEST (Dyn_repro_no_nan_extremes)
{
    ParamSnapshot snap;
    snap.agc = snap.comp = snap.limit = snap.dialog = snap.pump = 1.0f;
    snap.relChar = 1.0f;
    snap.width = 1.0f;
    snap.smallSpeaker = 1.0f;
    snap.monoMode = MonoMode::mono;
    snap.monoLaw = MonoLaw::minus3dB;

    PeriodDynamics dyn;
    Reproduction rep;
    dyn.prepare ({ kSr, kBlock, 2 });
    rep.prepare ({ kSr, kBlock, 2 });

    const int n = (int) kSr;
    juce::AudioBuffer<float> buf (2, n);
    for (int c = 0; c < 2; ++c)
    {
        auto* d = buf.getWritePointer (c);
        for (int i = 0; i < n; ++i)   // full-scale square, ~200 Hz
            d[i] = std::sin (2.0 * M_PI * 200.0 * i / kSr) >= 0.0 ? 1.0f : -1.0f;
    }
    processBlocks (dyn, buf, snap);
    processBlocks (rep, buf, snap);

    for (int c = 0; c < 2; ++c)
    {
        const auto* d = buf.getReadPointer (c);
        CHECK (! vfatest::hasNanOrInf (d, n));
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
            peak = std::max (peak, std::abs (d[i]));
        CHECK_MSG (peak <= 4.0f, "output blew up at extremes");
    }
}

VFA_TEST (Dyn_gr_meter_reports)
{
    auto snap = dynSnap();
    snap.comp = 0.8f;
    snap.limit = 0.8f;

    PeriodDynamics dyn;
    dyn.prepare ({ kSr, kBlock, 1 });
    juce::AudioBuffer<float> buf (1, (int) kSr);
    fillSine (buf, 1000.0, dbToGain (-6.0f));
    processBlocks (dyn, buf, snap);

    // Positive convention: dB of reduction currently applied (comp+limiter).
    const float gr = dyn.currentGainReductionDb();
    CHECK_MSG (gr >= 2.0f, "GR meter under-reports");
    CHECK_MSG (gr < 30.0f, "GR meter implausibly high");
}
