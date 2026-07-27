#include "../Harness/VfaTest.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/Transport/WowFlutterEngine.h"

using namespace vfa::dsp;

namespace
{
constexpr double kSr = 48000.0;

ParamSnapshot transportSnap (float wow, float flutter, float drift, float scrape,
                             TransportQuality quality = TransportQuality::studio)
{
    ParamSnapshot s;
    s.wow = wow;
    s.flutter = flutter;
    s.drift = drift;
    s.scrape = scrape;
    s.stereoLink = true;
    s.transportQuality = quality;
    return s;
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

// Deterministic seeded noise, identical on every channel.
void fillNoise (juce::AudioBuffer<float>& buf, uint64_t seed, float amp)
{
    Rng rng (seed);
    for (int i = 0; i < buf.getNumSamples(); ++i)
    {
        const float v = amp * rng.nextBipolar();
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            buf.getWritePointer (ch)[i] = v;
    }
}

// Process in host-like blocks.
void processAll (WowFlutterEngine& eng, juce::AudioBuffer<float>& buf,
                 const ParamSnapshot& snap, float depthScale)
{
    const int n = buf.getNumSamples();
    for (int pos = 0; pos < n; pos += 512)
    {
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), buf.getNumChannels(),
                                       pos, std::min (512, n - pos));
        eng.process (view, snap, depthScale);
    }
}

// Instantaneous frequency of a windowed segment from zero crossings, with
// linearly interpolated first/last positive-going crossing times (plain
// crossing counting quantizes to 1/(2T), too coarse for 0.1 % deviations).
double zcFrequency (const float* x, int len, double sampleRate)
{
    double first = -1.0, last = -1.0;
    int count = 0;
    for (int i = 1; i < len; ++i)
        if (x[i - 1] <= 0.0f && x[i] > 0.0f)
        {
            const double t = (i - 1) + (0.0 - x[i - 1]) / ((double) x[i] - (double) x[i - 1]);
            if (count == 0)
                first = t;
            last = t;
            ++count;
        }
    return count >= 2 ? sampleRate * (count - 1) / (last - first) : 0.0;
}
} // namespace

VFA_TEST (WowFlutter_depth_calibration)
{
    // wow = 0.5 targets 0.1 % peak speed deviation (DSP_SPEC §5); generous
    // tolerance for the multi-component/jittered modulator.
    WowFlutterEngine eng;
    eng.prepare ({ kSr, 512, 2 });
    eng.setSeed (1234);
    eng.reset();
    const auto snap = transportSnap (0.5f, 0.0f, 0.0f, 0.0f);

    const int n = 4 * 48000;
    juce::AudioBuffer<float> buf (2, n);
    fillSine (buf, 1000.0, 0.5f);
    processAll (eng, buf, snap, 1.0f);

    const auto* d = buf.getReadPointer (0);
    CHECK (! vfatest::hasNanOrInf (d, n));

    const int win = 12000, hop = 6000;             // 250 ms windows
    double maxDev = 0.0;
    for (int s = 36000; s + win <= n; s += hop)    // skip 0.75 s settle
        maxDev = std::max (maxDev, std::abs (zcFrequency (d + s, win, kSr) - 1000.0));
    const double frac = maxDev / 1000.0;
    char msg[128];
    std::snprintf (msg, sizeof (msg), "peak speed deviation %.4f %%", frac * 100.0);
    CHECK_MSG (frac >= 0.0004 && frac <= 0.0025, msg);
}

VFA_TEST (WowFlutter_spectrum_split)
{
    WowFlutterEngine eng;
    eng.prepare ({ kSr, 512, 1 });
    eng.setSeed (2024);
    eng.reset();
    auto snap = transportSnap (0.6f, 0.6f, 0.0f, 0.0f);
    snap.wowRateHz = 0.65f;
    snap.flutterRateHz = 24.0f;

    const int n = 8 * 48000;
    juce::AudioBuffer<float> buf (1, n);
    fillSine (buf, 1000.0, 0.5f);
    processAll (eng, buf, snap, 1.0f);

    // Deviation series: sliding 50 ms windows, 4 ms hop -> 250 Hz series rate
    // (Nyquist 125 Hz comfortably covers the 40-60 Hz control band).
    const auto* d = buf.getReadPointer (0);
    const int win = 2400, hop = 192;
    const double devFs = kSr / hop;
    std::vector<float> dev;
    for (int s = 36000; s + win <= n; s += hop)
        dev.push_back ((float) (zcFrequency (d + s, win, kSr) - 1000.0));
    double mean = 0.0;
    for (float v : dev)
        mean += v;
    mean /= (double) dev.size();
    for (float& v : dev)
        v -= (float) mean;

    auto bandPowerDb = [&] (std::initializer_list<double> freqs)
    {
        double p = 0.0;
        for (double f : freqs)
        {
            const double a = vfatest::goertzelAmplitude (dev.data(), (int) dev.size(), f, devFs);
            p += a * a;
        }
        return 10.0 * std::log10 (std::max (p / (double) freqs.size(), 1.0e-18));
    };
    const double lowDb  = bandPowerDb ({ 0.65, 1.25, 2.0, 3.0 });    // wow region (< 5 Hz)
    const double midDb  = bandPowerDb ({ 21.0, 23.0, 25.0, 27.0 });  // flutter region (15-35 Hz)
    const double highDb = bandPowerDb ({ 42.0, 47.0, 52.0, 57.0 });  // 40-60 Hz control band
    char msg[160];
    std::snprintf (msg, sizeof (msg), "low %.1f dB, mid %.1f dB, high %.1f dB",
                   lowDb, midDb, highDb);
    CHECK_MSG (lowDb >= highDb + 6.0, msg);
    CHECK_MSG (midDb >= highDb + 6.0, msg);
}

VFA_TEST (WowFlutter_bounded_and_no_discontinuity)
{
    WowFlutterEngine eng;
    eng.prepare ({ kSr, 512, 2 });
    const auto snap = transportSnap (1.0f, 1.0f, 1.0f, 1.0f, TransportQuality::damaged);

    // Continuity on a 100 Hz sine at extreme depth (depthScale 3).
    const int n = 4 * 48000;
    juce::AudioBuffer<float> buf (2, n);
    fillSine (buf, 100.0, 0.5f);
    eng.setSeed (7);
    eng.reset();
    processAll (eng, buf, snap, 3.0f);
    for (int ch = 0; ch < 2; ++ch)
        CHECK (! vfatest::hasNanOrInf (buf.getReadPointer (ch), n));
    const auto* d = buf.getReadPointer (0);
    double maxStep = 0.0;
    for (int i = 1; i < n; ++i)
        maxStep = std::max (maxStep, (double) std::abs (d[i] - d[i - 1]));
    CHECK_MSG (maxStep < 0.2, "discontinuity in modulated output");

    // Level sanity: modulated-delay reading must preserve noise RMS (+-3 dB).
    juce::AudioBuffer<float> noise (2, n);
    fillNoise (noise, 555, 0.25f);
    const double inDb = vfatest::rmsDb (noise.getReadPointer (0) + 24000, n - 24000);
    eng.setSeed (7);
    eng.reset();
    processAll (eng, noise, snap, 3.0f);
    const double outDb = vfatest::rmsDb (noise.getReadPointer (0) + 24000, n - 24000);
    CHECK_NEAR (outDb, inDb, 3.0);
}

VFA_TEST (WowFlutter_seed_determinism)
{
    WowFlutterEngine eng;
    eng.prepare ({ kSr, 512, 2 });
    const auto snap = transportSnap (0.5f, 0.5f, 0.4f, 0.3f, TransportQuality::worn);

    const int n = 48000;
    juce::AudioBuffer<float> input (2, n);
    fillNoise (input, 99, 0.25f);

    auto run = [&] (uint64_t seedValue)
    {
        juce::AudioBuffer<float> b (2, n);
        for (int ch = 0; ch < 2; ++ch)
            b.copyFrom (ch, 0, input, ch, 0, n);
        eng.setSeed (seedValue);
        eng.reset();
        processAll (eng, b, snap, 1.0f);
        return b;
    };
    const auto a = run (42);
    const auto b = run (42);
    const auto c = run (43);
    for (int ch = 0; ch < 2; ++ch)
        CHECK (std::memcmp (a.getReadPointer (ch), b.getReadPointer (ch),
                            sizeof (float) * (size_t) n) == 0);
    CHECK (std::memcmp (a.getReadPointer (0), c.getReadPointer (0),
                        sizeof (float) * (size_t) n) != 0);
}

VFA_TEST (WowFlutter_stereo_link)
{
    WowFlutterEngine eng;
    eng.prepare ({ kSr, 512, 2 });
    auto snap = transportSnap (0.5f, 0.6f, 0.2f, 0.3f, TransportQuality::worn);

    const int n = 2 * 48000;
    auto run = [&] (bool linked)
    {
        juce::AudioBuffer<float> b (2, n);
        fillSine (b, 1000.0, 0.5f);            // identical L/R input
        snap.stereoLink = linked;
        eng.setSeed (5);
        eng.reset();
        processAll (eng, b, snap, 1.0f);
        return b;
    };

    const auto linked = run (true);
    CHECK (std::memcmp (linked.getReadPointer (0), linked.getReadPointer (1),
                        sizeof (float) * (size_t) n) == 0);

    const auto unlinked = run (false);
    double maxDiff = 0.0;
    for (int i = 0; i < n; ++i)
        maxDiff = std::max (maxDiff, (double) std::abs (unlinked.getReadPointer (0)[i]
                                                        - unlinked.getReadPointer (1)[i]));
    CHECK_MSG (maxDiff > 1.0e-6, "unlinked channels did not decorrelate");
}

VFA_TEST (WowFlutter_zero_depth_pure_delay)
{
    WowFlutterEngine eng;
    eng.prepare ({ kSr, 512, 2 });
    eng.setSeed (1);
    eng.reset();
    const auto snap = transportSnap (0.0f, 0.0f, 0.0f, 0.0f);

    const int n = 48000, delay = 576;          // 12 ms centre at 48 kHz
    juce::AudioBuffer<float> input (2, n);
    fillNoise (input, 321, 0.5f);
    juce::AudioBuffer<float> out (2, n);
    for (int ch = 0; ch < 2; ++ch)
        out.copyFrom (ch, 0, input, ch, 0, n);
    processAll (eng, out, snap, 1.0f);

    const auto* in0 = input.getReadPointer (0);
    const auto* out0 = out.getReadPointer (0);
    double maxErr = 0.0;
    for (int i = delay; i < n; ++i)
        maxErr = std::max (maxErr, (double) std::abs (out0[i] - in0[i - delay]));
    CHECK_MSG (maxErr <= 1.0e-4, "zero depth is not a pure centre delay");
}
