// Medium model tests: MagneticModel (DSP_SPEC §3) and BroadcastChain (§7).
// Bandwidth checks probe a few Goertzel bins instead of full sweeps: the
// table corner must sit between -6 and -1 dB re 1 kHz (i.e. within ~1/3
// octave of a -3 dB point) and an octave beyond it must be < -6 dB.
#include "../Harness/VfaTest.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/Magnetic/MagneticModel.h"
#include "DSP/Broadcast/BroadcastChain.h"

using namespace vfa::dsp;

namespace
{
constexpr double kSr      = 48000.0;
constexpr int    kWarm    = 24000;   // settle envelopes / oversamplers
constexpr int    kAnalyze = 48000;   // 1 s analysis -> 1 Hz Goertzel bins
constexpr int    kTotal   = kWarm + kAnalyze;

ParamSnapshot magSnap (Medium m, float sat, float bump,
                       TapeSpeed speed = TapeSpeed::ips15)
{
    ParamSnapshot s;
    s.medium = m;
    s.magSat = sat;
    s.headBump = bump;
    s.tapeSpeed = speed;
    return s;
}

ParamSnapshot bcSnap (Medium m)
{
    ParamSnapshot s;
    s.medium = m;
    return s;
}

void fillSine (juce::AudioBuffer<float>& buf, double hz, float amp,
               bool invertRight = false)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        const float sign = (invertRight && ch == 1) ? -1.0f : 1.0f;
        for (int i = 0; i < buf.getNumSamples(); ++i)
            d[i] = sign * amp * (float) std::sin (2.0 * M_PI * hz * i / kSr);
    }
}

template <typename ProcessFn>
void runInBlocks (juce::AudioBuffer<float>& buf, ProcessFn&& fn)
{
    const int n = buf.getNumSamples();
    for (int pos = 0; pos < n; pos += 512)
    {
        const int len = std::min (512, n - pos);
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(),
                                       buf.getNumChannels(), pos, len);
        fn (view);
    }
}

// Steady-state output level (dBFS) of the magnetic model at hz.
double magOutDb (const ParamSnapshot& snap, double hz, float amp,
                 float extraDriveDb = 0.0f)
{
    MagneticModel m;
    m.prepare ({ kSr, 512, 1 });
    m.reset();
    juce::AudioBuffer<float> buf (1, kTotal);
    fillSine (buf, hz, amp);
    runInBlocks (buf, [&] (juce::AudioBuffer<float>& v) { m.process (v, snap, extraDriveDb); });
    return vfatest::amplitudeToDb (
        vfatest::goertzelAmplitude (buf.getReadPointer (0) + kWarm, kAnalyze, hz, kSr));
}

// Response (output re input level) in dB.
double magRespDb (const ParamSnapshot& snap, double hz, float amp)
{
    return magOutDb (snap, hz, amp) - vfatest::amplitudeToDb (amp);
}

double magThdPercent (const ParamSnapshot& snap, float amp)
{
    MagneticModel m;
    m.prepare ({ kSr, 512, 1 });
    m.reset();
    juce::AudioBuffer<float> buf (1, kTotal);
    fillSine (buf, 1000.0, amp);
    runInBlocks (buf, [&] (juce::AudioBuffer<float>& v) { m.process (v, snap, 0.0f); });
    return vfatest::thdPercent (buf.getReadPointer (0) + kWarm, kAnalyze, 1000.0, kSr);
}

// Steady-state output level (dBFS, left channel) of the broadcast chain.
double bcOutDb (const ParamSnapshot& snap, double hz, float amp, int numCh,
                float intensity, bool invertRight = false)
{
    BroadcastChain b;
    b.prepare ({ kSr, 512, numCh });
    b.reset();
    juce::AudioBuffer<float> buf (numCh, kTotal);
    fillSine (buf, hz, amp, invertRight);
    runInBlocks (buf, [&] (juce::AudioBuffer<float>& v) { b.process (v, snap, intensity); });
    return vfatest::amplitudeToDb (
        vfatest::goertzelAmplitude (buf.getReadPointer (0) + kWarm, kAnalyze, hz, kSr));
}

double bcRespDb (const ParamSnapshot& snap, double hz, float amp, int numCh,
                 float intensity)
{
    return bcOutDb (snap, hz, amp, numCh, intensity) - vfatest::amplitudeToDb (amp);
}

void checkCorner (double relDb, const char* what)
{
    char msg[128];
    std::snprintf (msg, sizeof (msg), "%s: %.2f dB (want -6..-1)", what, relDb);
    CHECK_MSG (relDb > -6.0 && relDb < -1.0, msg);
}
} // namespace

// 1. Magnetic film bandwidth: 40 Hz-14 kHz (-3 dB, DSP_SPEC §3). headBump 0
//    so the band edges are isolated from the bump.
VFA_TEST (Medium_magnetic_bandwidth)
{
    const auto snap = magSnap (Medium::magneticFilm, 0.2f, 0.0f);
    const float amp = 0.05f;
    const double ref = magRespDb (snap, 1000.0, amp);
    checkCorner (magRespDb (snap, 40.0, amp) - ref, "film 40 Hz corner");
    checkCorner (magRespDb (snap, 14000.0, amp) - ref, "film 14 kHz corner");
    CHECK_MSG (magRespDb (snap, 20.0, amp) - ref < -6.0, "film 20 Hz < -6 dB");
}

// 2. Field tape 50 Hz-12 kHz, consumer 60 Hz-9 kHz; consumer additionally
//    darker at 8 kHz (azimuth-style HF shelf loss) by >= 1.5 dB.
VFA_TEST (Medium_fieldtape_and_consumer_bandwidth)
{
    const float amp = 0.05f;

    const auto tape = magSnap (Medium::fieldTape, 0.2f, 0.0f);
    const double tapeRef = magRespDb (tape, 1000.0, amp);
    checkCorner (magRespDb (tape, 50.0, amp) - tapeRef, "tape 50 Hz corner");
    checkCorner (magRespDb (tape, 12000.0, amp) - tapeRef, "tape 12 kHz corner");
    CHECK_MSG (magRespDb (tape, 25.0, amp) - tapeRef < -6.0, "tape 25 Hz < -6 dB");

    const auto cons = magSnap (Medium::consumer, 0.2f, 0.0f);
    const double consRef = magRespDb (cons, 1000.0, amp);
    checkCorner (magRespDb (cons, 60.0, amp) - consRef, "consumer 60 Hz corner");
    checkCorner (magRespDb (cons, 9000.0, amp) - consRef, "consumer 9 kHz corner");
    CHECK_MSG (magRespDb (cons, 30.0, amp) - consRef < -6.0, "consumer 30 Hz < -6 dB");
    CHECK_MSG (magRespDb (cons, 18000.0, amp) - consRef < -6.0, "consumer 18 kHz < -6 dB");

    const double tape8k = magRespDb (tape, 8000.0, amp) - tapeRef;
    const double cons8k = magRespDb (cons, 8000.0, amp) - consRef;
    char msg[128];
    std::snprintf (msg, sizeof (msg), "consumer 8 kHz %.2f dB vs tape %.2f dB", cons8k, tape8k);
    CHECK_MSG (tape8k - cons8k >= 1.5, msg);
}

// 3. Head bump tracks tape speed (7.5 ips -> 70 Hz, 30 ips -> 40 Hz). The
//    bump is measured differentially (headBump 1 vs 0) so the fixed lower
//    band edge cancels out of the "local boost" figure.
VFA_TEST (Medium_headbump_tracks_speed)
{
    const float amp = 0.05f;
    auto boostAt = [amp] (TapeSpeed speed, double hz)
    {
        return magOutDb (magSnap (Medium::magneticFilm, 0.1f, 1.0f, speed), hz, amp)
             - magOutDb (magSnap (Medium::magneticFilm, 0.1f, 0.0f, speed), hz, amp);
    };
    const double b75  = boostAt (TapeSpeed::ips7_5, 70.0)
                      - boostAt (TapeSpeed::ips7_5, 200.0);
    const double b30  = boostAt (TapeSpeed::ips30, 40.0)
                      - boostAt (TapeSpeed::ips30, 200.0);
    CHECK_MSG (b75 >= 1.0, "7.5 ips bump >= +1 dB @ 70 Hz re 200 Hz");
    CHECK_MSG (b30 >= 1.0, "30 ips bump >= +1 dB @ 40 Hz re 200 Hz");

    // The bump frequency moves: with the bump engaged, 70 Hz is hotter at
    // 7.5 ips than at 30 ips (whose bump sits down at 40 Hz).
    const double at70slow = magOutDb (magSnap (Medium::magneticFilm, 0.1f, 1.0f, TapeSpeed::ips7_5), 70.0, amp);
    const double at70fast = magOutDb (magSnap (Medium::magneticFilm, 0.1f, 1.0f, TapeSpeed::ips30), 70.0, amp);
    CHECK_MSG (at70slow > at70fast, "bump frequency tracks speed");
}

// 4. THD at 1 kHz rises monotonically with magSat and stays sane; also pins
//    the DSP_SPEC calibration point (-18 dBFS @ magSat 0.4 -> ~1-2 % THD).
VFA_TEST (Medium_magnetic_thd_rises_with_sat)
{
    const float amp14 = dbToGain (-14.0f);
    const double t1 = magThdPercent (magSnap (Medium::magneticFilm, 0.1f, 0.0f), amp14);
    const double t4 = magThdPercent (magSnap (Medium::magneticFilm, 0.4f, 0.0f), amp14);
    const double t8 = magThdPercent (magSnap (Medium::magneticFilm, 0.8f, 0.0f), amp14);
    char msg[128];
    std::snprintf (msg, sizeof (msg), "THD %% at -14 dBFS: %.2f / %.2f / %.2f", t1, t4, t8);
    CHECK_MSG (t1 < t4 && t4 < t8, msg);
    CHECK_MSG (t8 < 15.0, msg);

    const double cal = magThdPercent (magSnap (Medium::magneticFilm, 0.4f, 0.0f),
                                      dbToGain (-18.0f));
    std::snprintf (msg, sizeof (msg), "calibration THD %.2f %% (want ~1-2)", cal);
    CHECK_MSG (cal > 0.7 && cal < 2.5, msg);
}

// 5. Level-dependent HF loss (bias / self-erasure): at magSat 0.6 a hot
//    10 kHz tone loses >= 1.5 dB more (relative to its input level) than a
//    quiet one. Generic clippers do the opposite.
VFA_TEST (Medium_magnetic_level_dependent_hf_loss)
{
    const auto snap = magSnap (Medium::magneticFilm, 0.6f, 0.0f);
    const double relQuiet = magRespDb (snap, 10000.0, dbToGain (-30.0f));
    const double relHot   = magRespDb (snap, 10000.0, dbToGain (-8.0f));
    char msg[128];
    std::snprintf (msg, sizeof (msg), "10 kHz rel loss: quiet %.2f dB, hot %.2f dB",
                   relQuiet, relHot);
    CHECK_MSG (relQuiet - relHot >= 1.5, msg);
}

// 6. Broadcast band limits (§3) and receiver ripple: mono 100 Hz-5 kHz,
//    stereo (MTS) 50 Hz-14 kHz.
VFA_TEST (Medium_broadcast_bandwidth_and_ripple)
{
    const float amp = 0.05f;

    const auto mono = bcSnap (Medium::broadcastMono);
    const double monoRef = bcRespDb (mono, 1000.0, amp, 1, 1.0f);
    checkCorner (bcRespDb (mono, 100.0, amp, 1, 1.0f) - monoRef, "mono 100 Hz corner");
    checkCorner (bcRespDb (mono, 5000.0, amp, 1, 1.0f) - monoRef, "mono 5 kHz corner");
    CHECK_MSG (bcRespDb (mono, 50.0, amp, 1, 1.0f) - monoRef < -6.0, "mono 50 Hz < -6 dB");
    CHECK_MSG (bcRespDb (mono, 10000.0, amp, 1, 1.0f) - monoRef < -12.0, "mono 10 kHz < -12 dB");

    const auto st = bcSnap (Medium::broadcastStereo);
    const double stRef = bcRespDb (st, 1000.0, amp, 2, 1.0f);
    const double st10k = bcRespDb (st, 10000.0, amp, 2, 1.0f) - stRef;
    char msg[128];
    std::snprintf (msg, sizeof (msg), "stereo 10 kHz %.2f dB (want -4..+1)", st10k);
    CHECK_MSG (st10k > -4.0 && st10k < 1.0, msg);

    // IF ripple signature: +1 dB bump near 2.5 kHz vs the dip side near 4 kHz.
    const double ripple = bcRespDb (st, 2500.0, amp, 2, 1.0f)
                        - bcRespDb (st, 4000.0, amp, 2, 1.0f);
    std::snprintf (msg, sizeof (msg), "ripple 2.5k vs 4k: %.2f dB", ripple);
    CHECK_MSG (ripple >= 0.3 && ripple <= 3.0, msg);
}

// 7. Pre-emph -> clip -> de-emph signature: level-dependent gain reduction
//    hits an 8 kHz tone (pre-emphasized into the deviation limit) harder
//    than a 400 Hz tone. Measured differentially (-10 dBFS vs -30 dBFS) so
//    the linear band shape cancels out.
VFA_TEST (Medium_broadcast_hf_compression)
{
    const auto st = bcSnap (Medium::broadcastStereo);
    auto levelGrDb = [&st] (double hz)
    {
        const double relHot   = bcRespDb (st, hz, dbToGain (-10.0f), 2, 1.0f);
        const double relQuiet = bcRespDb (st, hz, dbToGain (-30.0f), 2, 1.0f);
        return relHot - relQuiet;    // negative = level-dependent compression
    };
    const double gr400 = levelGrDb (400.0);
    const double gr8k  = levelGrDb (8000.0);
    char msg[128];
    std::snprintf (msg, sizeof (msg), "level-dependent GR: 400 Hz %.2f dB, 8 kHz %.2f dB",
                   gr400, gr8k);
    CHECK_MSG (gr400 - gr8k >= 0.5, msg);
}

// 8. MTS stereo: the side (L-R) path loses HF re the mid path (~-6 dB shelf
//    at 10 kHz -> a few dB down at 8 kHz).
VFA_TEST (Medium_broadcast_stereo_side_hf_loss)
{
    const auto st = bcSnap (Medium::broadcastStereo);
    const float amp = dbToGain (-14.0f);
    const double mid  = bcOutDb (st, 8000.0, amp, 2, 1.0f, false); // L == R
    const double side = bcOutDb (st, 8000.0, amp, 2, 1.0f, true);  // L == -R
    const double extraLoss = mid - side;
    char msg[128];
    std::snprintf (msg, sizeof (msg), "side extra loss @ 8 kHz: %.2f dB (want 1..8)", extraLoss);
    CHECK_MSG (extraLoss > 1.0 && extraLoss < 8.0, msg);
}

// 9. Aliasing bounded at Standard (2x) quality: a driven 15 kHz tone must not
//    leak folded products (45 kHz -> 3 kHz, 30 kHz -> 18 kHz at 48 kHz) above
//    -40 dB re the fundamental.
VFA_TEST (Medium_aliasing_bounded)
{
    MagneticModel m;
    m.prepare ({ kSr, 512, 1 });
    m.reset();
    const auto snap = magSnap (Medium::magneticFilm, 0.8f, 0.0f);
    juce::AudioBuffer<float> buf (1, kTotal);
    fillSine (buf, 15000.0, dbToGain (-12.0f));
    runInBlocks (buf, [&] (juce::AudioBuffer<float>& v) { m.process (v, snap, 0.0f); });

    const float* d = buf.getReadPointer (0) + kWarm;
    const double fund = vfatest::amplitudeToDb (
        vfatest::goertzelAmplitude (d, kAnalyze, 15000.0, kSr));
    const double a3k = vfatest::amplitudeToDb (
        vfatest::goertzelAmplitude (d, kAnalyze, 3000.0, kSr));
    const double a18k = vfatest::amplitudeToDb (
        vfatest::goertzelAmplitude (d, kAnalyze, 18000.0, kSr));
    CHECK_MSG (fund > -40.0, "fundamental survives the chain");
    char msg[128];
    std::snprintf (msg, sizeof (msg), "aliases re fundamental: 3 kHz %.1f dB, 18 kHz %.1f dB",
                   a3k - fund, a18k - fund);
    CHECK_MSG (a3k - fund < -40.0, msg);
    CHECK_MSG (a18k - fund < -40.0, msg);
}

// 10. No NaN/Inf and bounded output for a full-scale square with every knob
//     maxed, on both modules; zero-length blocks must be harmless too.
VFA_TEST (Medium_no_nan_extremes)
{
    auto fillSquare = [] (juce::AudioBuffer<float>& buf)
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            for (int i = 0; i < buf.getNumSamples(); ++i)
                d[i] = ((i / 24) & 1) ? -1.0f : 1.0f;   // 1 kHz full-scale square
        }
    };
    auto checkBuffer = [] (const juce::AudioBuffer<float>& buf, const char* what)
    {
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const float* d = buf.getReadPointer (ch);
            CHECK_MSG (! vfatest::hasNanOrInf (d, buf.getNumSamples()), what);
            float peak = 0.0f;
            for (int i = 0; i < buf.getNumSamples(); ++i)
                peak = std::max (peak, std::abs (d[i]));
            char msg[96];
            std::snprintf (msg, sizeof (msg), "%s peak %.2f (want < 4)", what, peak);
            CHECK_MSG (peak < 4.0f, msg);
        }
    };

    auto snap = magSnap (Medium::consumer, 1.0f, 1.0f, TapeSpeed::ips3_75);
    snap.quality = Quality::high;

    MagneticModel m;
    m.prepare ({ kSr, 512, 2 });
    m.reset();
    juce::AudioBuffer<float> magBuf (2, 48000);
    fillSquare (magBuf);
    runInBlocks (magBuf, [&] (juce::AudioBuffer<float>& v) { m.process (v, snap, 12.0f); });
    checkBuffer (magBuf, "magnetic extremes");

    auto bsnap = bcSnap (Medium::broadcastStereo);
    bsnap.quality = Quality::high;

    BroadcastChain b;
    b.prepare ({ kSr, 512, 2 });
    b.reset();
    juce::AudioBuffer<float> bcBuf (2, 48000);
    fillSquare (bcBuf);
    runInBlocks (bcBuf, [&] (juce::AudioBuffer<float>& v) { b.process (v, bsnap, 1.0f); });
    checkBuffer (bcBuf, "broadcast extremes");

    // Zero-length blocks are a no-op, not a crash.
    juce::AudioBuffer<float> empty (2, 0);
    m.process (empty, snap, 0.0f);
    b.process (empty, bsnap, 1.0f);
    CHECK (empty.getNumSamples() == 0);
}
