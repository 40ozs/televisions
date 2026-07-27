// Noise & artifact engine tests (DSP_SPEC §8/§9): calibration, spectra,
// event densities, determinism, correlation, ducking, dropouts, no looping.
#include "../Harness/VfaTest.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/Noise/NoiseArtifactEngine.h"

#include <cstring>

using namespace vfa::dsp;

namespace
{
constexpr double kSr = 48000.0;

// Snapshot with every noise source off; tests enable one source at a time.
ParamSnapshot quietSnap()
{
    ParamSnapshot s;
    s.medium = Medium::magneticFilm;
    s.nsHiss = s.nsCell = s.nsBroadcast = 0.0f;
    s.nsHum = s.nsBuzz = 0.0f;
    s.nsHumHarm = 0.0f;
    s.nsCrackle = s.nsDirt = s.nsDropout = 0.0f;
    s.nsProjector = s.nsPrintThrough = 0.0f;
    s.nsDuck = 0.0f;
    s.nsWidth = 0.5f;
    s.nsSilence = true;
    s.humFreqHz = 60.0f;
    return s;
}

void render (NoiseArtifactEngine& eng, juce::AudioBuffer<float>& buf,
             const ParamSnapshot& snap, float extraDb = 0.0f)
{
    const int n = buf.getNumSamples();
    for (int pos = 0; pos < n; pos += 512)
    {
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(),
                                       buf.getNumChannels(), pos,
                                       std::min (512, n - pos));
        eng.process (view, snap, extraDb);
    }
}

double rmsDbRange (const juce::AudioBuffer<float>& buf, int ch, int start, int len)
{
    return vfatest::rmsDb (buf.getReadPointer (ch) + start, len);
}

double corrZeroLag (const float* a, const float* b, int n)
{
    double ab = 0, aa = 0, bb = 0;
    for (int i = 0; i < n; ++i)
    {
        ab += (double) a[i] * b[i];
        aa += (double) a[i] * a[i];
        bb += (double) b[i] * b[i];
    }
    return ab / std::sqrt (std::max (aa * bb, 1.0e-30));
}

// Count super-threshold groups separated by at least `refractory` samples.
int countEvents (const float* x, int n, float thr, int refractory)
{
    int count = 0, last = -refractory - 1;
    for (int i = 0; i < n; ++i)
        if (std::abs (x[i]) > thr)
        {
            if (i - last > refractory)
                ++count;
            last = i;
        }
    return count;
}

void fillSine (juce::AudioBuffer<float>& buf, double hz, float amp)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
            d[i] = amp * (float) std::sin (2.0 * M_PI * hz * i / kSr);
    }
}
} // namespace

// 1. Hiss RMS calibration: magneticFilm SNR 62 dB (research doc §1.5:
//    studio/35 mm magnetic 60-75 dB) below -18 dBFS -> -80 dBFS at param 0.5
//    (+-3 dB); param 1.0 sits 10-25 dB above that.
VFA_TEST (Noise_hiss_level_calibration)
{
    NoiseArtifactEngine eng;
    eng.prepare ({ kSr, 512, 2 });
    eng.setSeed (4242);
    auto snap = quietSnap();
    snap.nsHiss = 0.5f;

    juce::AudioBuffer<float> buf (2, 96000);
    buf.clear();
    eng.reset();
    render (eng, buf, snap);
    const double dbHalf = rmsDbRange (buf, 0, 12000, 84000);
    CHECK_NEAR (dbHalf, -80.0, 3.0);

    snap.nsHiss = 1.0f;
    buf.clear();
    eng.reset();
    render (eng, buf, snap);
    const double dbFull = rmsDbRange (buf, 0, 12000, 84000);
    CHECK_MSG (dbFull - dbHalf >= 10.0 && dbFull - dbHalf <= 25.0,
               "hiss 1.0 must sit 10-25 dB above 0.5");
}

// 2. Hum lands exactly on the mains fundamental, and nsHumHarm brings up
//    the 2nd harmonic.
VFA_TEST (Noise_hum_frequency_exact)
{
    NoiseArtifactEngine eng;
    eng.prepare ({ kSr, 512, 2 });
    eng.setSeed (11);
    auto snap = quietSnap();
    snap.nsHum = 0.5f;
    snap.nsHumHarm = 0.5f;

    juce::AudioBuffer<float> buf (2, 192000);
    for (float freq : { 60.0f, 50.0f })
    {
        snap.humFreqHz = freq;
        buf.clear();
        eng.reset();
        render (eng, buf, snap);
        const float* d = buf.getReadPointer (0) + 24000;
        const int n = 168000;
        const double fund = vfatest::amplitudeToDb (
            vfatest::goertzelAmplitude (d, n, freq, kSr));
        const double below = vfatest::amplitudeToDb (
            vfatest::goertzelAmplitude (d, n, freq - 5.0, kSr));
        const double above = vfatest::amplitudeToDb (
            vfatest::goertzelAmplitude (d, n, freq + 5.0, kSr));
        CHECK_MSG (fund - below >= 20.0, "fundamental must dominate -5 Hz bin");
        CHECK_MSG (fund - above >= 20.0, "fundamental must dominate +5 Hz bin");
        const double h2 = vfatest::amplitudeToDb (
            vfatest::goertzelAmplitude (d, n, 2.0 * freq, kSr));
        CHECK_MSG (fund - h2 <= 30.0, "2nd harmonic present at nsHumHarm=0.5");
    }
}

// 3. Sync buzz: odd harmonics of 59.94 Hz within 25 dB of the fundamental.
VFA_TEST (Noise_buzz_odd_harmonics)
{
    NoiseArtifactEngine eng;
    eng.prepare ({ kSr, 512, 2 });
    eng.setSeed (12);
    auto snap = quietSnap();
    snap.nsBuzz = 0.5f;
    snap.humFreqHz = 60.0f;   // -> 59.94 Hz NTSC-style buzz

    juce::AudioBuffer<float> buf (2, 192000);
    buf.clear();
    eng.reset();
    render (eng, buf, snap);
    const float* d = buf.getReadPointer (0) + 24000;
    const int n = 168000;
    const double f0 = 59.94;
    const double fund = vfatest::amplitudeToDb (vfatest::goertzelAmplitude (d, n, f0, kSr));
    const double h3 = vfatest::amplitudeToDb (vfatest::goertzelAmplitude (d, n, 3.0 * f0, kSr));
    const double h5 = vfatest::amplitudeToDb (vfatest::goertzelAmplitude (d, n, 5.0 * f0, kSr));
    CHECK_MSG (fund - h3 <= 25.0, "3rd harmonic within 25 dB of fundamental");
    CHECK_MSG (fund - h5 <= 25.0, "5th harmonic within 25 dB of fundamental");
}

// 4. Crackle density scales with amount; zero amount emits nothing.
VFA_TEST (Noise_crackle_density_scales)
{
    auto count = [] (float amount)
    {
        NoiseArtifactEngine eng;
        eng.prepare ({ kSr, 512, 2 });
        eng.setSeed (2024);
        auto snap = quietSnap();
        snap.nsCrackle = amount;
        juce::AudioBuffer<float> buf (2, 240000);   // 5 s
        buf.clear();
        eng.reset();
        render (eng, buf, snap);
        return countEvents (buf.getReadPointer (0), 240000, 0.001f, 240);
    };
    const int low = count (0.3f), high = count (0.8f), none = count (0.0f);
    CHECK_MSG (none == 0, "nsCrackle=0 must be fully silent");
    CHECK_MSG (low > 0, "nsCrackle=0.3 must produce events");
    char msg[96];
    std::snprintf (msg, sizeof (msg), "events low=%d high=%d", low, high);
    CHECK_MSG (high >= 3 * low, msg);
}

// 5. Full determinism from the seed (DSP_SPEC §9): reset + same seed is
//    bit-exact; a different seed differs.
VFA_TEST (Noise_seed_determinism)
{
    NoiseArtifactEngine eng;
    eng.prepare ({ kSr, 512, 2 });
    auto snap = quietSnap();
    snap.nsHiss = snap.nsCell = snap.nsBroadcast = 0.4f;
    snap.nsHum = snap.nsHumHarm = snap.nsBuzz = 0.4f;
    snap.nsCrackle = snap.nsDirt = snap.nsDropout = 0.4f;
    snap.nsProjector = snap.nsPrintThrough = 0.4f;
    snap.nsDuck = 0.4f;
    snap.nsWidth = 0.4f;

    juce::AudioBuffer<float> a (2, 48000), b (2, 48000), c (2, 48000);
    eng.setSeed (777);
    eng.reset();
    a.clear();
    render (eng, a, snap, 3.0f);

    eng.reset();
    b.clear();
    render (eng, b, snap, 3.0f);

    eng.setSeed (778);
    eng.reset();
    c.clear();
    render (eng, c, snap, 3.0f);

    bool identical = true;
    for (int ch = 0; ch < 2; ++ch)
        identical = identical
            && std::memcmp (a.getReadPointer (ch), b.getReadPointer (ch),
                            sizeof (float) * 48000) == 0;
    CHECK_MSG (identical, "same seed after reset must be bit-exact");

    bool differs = false;
    for (int i = 0; i < 48000 && ! differs; ++i)
        differs = a.getReadPointer (0)[i] != c.getReadPointer (0)[i];
    CHECK_MSG (differs, "different seed must change the output");
}

// 6. Stereo correlation: nsWidth collapses hiss toward mono; hum is always
//    fully correlated across channels.
VFA_TEST (Noise_correlation_width)
{
    NoiseArtifactEngine eng;
    eng.prepare ({ kSr, 512, 2 });
    eng.setSeed (31);
    auto snap = quietSnap();
    snap.nsHiss = 0.5f;

    juce::AudioBuffer<float> buf (2, 96000);
    auto corrFor = [&] (const ParamSnapshot& s)
    {
        buf.clear();
        eng.reset();
        render (eng, buf, s);
        return corrZeroLag (buf.getReadPointer (0) + 24000,
                            buf.getReadPointer (1) + 24000, 72000);
    };

    snap.nsWidth = 0.0f;
    CHECK_MSG (corrFor (snap) > 0.95, "width 0: hiss near-mono");
    snap.nsWidth = 1.0f;
    CHECK_MSG (corrFor (snap) < 0.4, "width 1: hiss decorrelated");

    snap.nsHiss = 0.0f;
    snap.nsHum = 0.5f;
    CHECK_MSG (corrFor (snap) > 0.95, "hum always correlated");
}

// 7. Ducking: loud program pulls continuous noise down; nsSilence=false
//    gates noise out entirely in true silence.
VFA_TEST (Noise_ducking_behavior)
{
    NoiseArtifactEngine eng;
    eng.prepare ({ kSr, 512, 2 });
    auto snap = quietSnap();
    snap.nsHiss = 0.6f;

    juce::AudioBuffer<float> program (2, 96000), buf (2, 96000);
    fillSine (program, 1000.0, dbToGain (-12.0f));

    auto addedNoiseDb = [&] (const ParamSnapshot& s, bool withProgram)
    {
        eng.setSeed (55);
        eng.reset();
        if (withProgram)
            for (int ch = 0; ch < 2; ++ch)
                buf.copyFrom (ch, 0, program, ch, 0, 96000);
        else
            buf.clear();
        render (eng, buf, s);
        if (withProgram)
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buf.getWritePointer (ch);
                const auto* p = program.getReadPointer (ch);
                for (int i = 0; i < 96000; ++i)
                    d[i] -= p[i];
            }
        return rmsDbRange (buf, 0, 48000, 48000);
    };

    snap.nsDuck = 1.0f;
    const double duckedDb = addedNoiseDb (snap, true);
    const double openDb = addedNoiseDb (snap, false);
    CHECK_MSG (duckedDb <= openDb - 6.0, "nsDuck=1: loud program ducks >= 6 dB");

    snap.nsDuck = 0.0f;
    const double noDuckProgram = addedNoiseDb (snap, true);
    const double noDuckSilence = addedNoiseDb (snap, false);
    CHECK_MSG (std::abs (noDuckProgram - noDuckSilence) < 2.0,
               "nsDuck=0: program must not change the noise level");

    snap.nsSilence = false;
    const double gatedDb = addedNoiseDb (snap, false);   // measures 1..2 s
    CHECK_MSG (gatedDb < -80.0, "nsSilence off: noise gates out in silence");
}

// 8. Dropouts gate the program: a -18 dBFS sine develops quiet 250 ms
//    windows; with nsDropout=0 the windows stay flat.
VFA_TEST (Noise_dropout_gates_program)
{
    auto windowSpreadDb = [] (float amount)
    {
        NoiseArtifactEngine eng;
        eng.prepare ({ kSr, 512, 2 });
        eng.setSeed (20250727);
        auto snap = quietSnap();
        snap.nsDropout = amount;
        juce::AudioBuffer<float> buf (2, 384000);   // 8 s
        fillSine (buf, 200.0, dbToGain (-18.0f));
        eng.reset();
        render (eng, buf, snap);
        const float* d = buf.getReadPointer (0);
        const int win = 12000;                      // 250 ms
        double minDb = 1.0e9, maxDb = -1.0e9;
        for (int start = 24000; start + win <= 384000; start += 3000)
        {
            const double db = vfatest::rmsDb (d + start, win);
            minDb = std::min (minDb, db);
            maxDb = std::max (maxDb, db);
        }
        return maxDb - minDb;
    };
    CHECK_MSG (windowSpreadDb (0.8f) >= 2.0, "nsDropout=0.8 must dip >= 2 dB");
    CHECK_MSG (windowSpreadDb (0.0f) <= 0.5, "nsDropout=0 must stay flat");
}

// 9. No repetition: procedural hiss must not correlate chunk-to-chunk over
//    30 s (no looped buffers, ADR-008).
VFA_TEST (Noise_no_repetition)
{
    NoiseArtifactEngine eng;
    eng.prepare ({ kSr, 512, 2 });
    eng.setSeed (99);
    auto snap = quietSnap();
    snap.nsHiss = 0.5f;

    juce::AudioBuffer<float> buf (2, 30 * 48000);
    buf.clear();
    eng.reset();
    render (eng, buf, snap);

    const float* d = buf.getReadPointer (0);
    double worst = 0.0;
    for (int k = 1; k < 30; ++k)
        worst = std::max (worst, std::abs (corrZeroLag (d, d + k * 48000, 48000)));
    char msg[64];
    std::snprintf (msg, sizeof (msg), "worst chunk correlation %.4f", worst);
    CHECK_MSG (worst < 0.2, msg);
}

// 10. Kitchen sink: everything maxed, floors lifted, full-scale square
//     program — output stays finite and bounded.
VFA_TEST (Noise_no_nan_extremes)
{
    NoiseArtifactEngine eng;
    eng.prepare ({ kSr, 512, 2 });
    eng.setSeed (5);
    auto snap = quietSnap();
    snap.nsHiss = snap.nsCell = snap.nsBroadcast = 1.0f;
    snap.nsHum = snap.nsHumHarm = snap.nsBuzz = 1.0f;
    snap.nsCrackle = snap.nsDirt = snap.nsDropout = 1.0f;
    snap.nsProjector = snap.nsPrintThrough = 1.0f;
    snap.nsDuck = 1.0f;
    snap.nsWidth = 1.0f;

    juce::AudioBuffer<float> buf (2, 96000);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        for (int i = 0; i < 96000; ++i)
            d[i] = std::sin (2.0 * M_PI * 200.0 * i / kSr) >= 0.0 ? 1.0f : -1.0f;
    }
    eng.reset();
    render (eng, buf, snap, 12.0f);

    for (int ch = 0; ch < 2; ++ch)
    {
        const float* d = buf.getReadPointer (ch);
        CHECK (! vfatest::hasNanOrInf (d, 96000));
        float peak = 0.0f;
        for (int i = 0; i < 96000; ++i)
            peak = std::max (peak, std::abs (d[i]));
        CHECK_MSG (peak < 4.0f, "output bounded under extreme settings");
    }
}
