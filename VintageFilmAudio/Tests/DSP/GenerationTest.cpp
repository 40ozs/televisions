#include "../Harness/VfaTest.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include "DSP/GenerationLoss/GenerationModel.h"
#include "DSP/GenerationLoss/ExactReferenceChain.h"

using namespace vfa::dsp;

namespace
{
constexpr double kSr = 48000.0;
constexpr float kProbeAmp = 0.1f;

ParamSnapshot genSnap (float generations, bool integerMode, float variability)
{
    ParamSnapshot s;
    s.medium = Medium::magneticFilm;
    s.generations = generations;
    s.genInteger = integerMode;
    s.genVariability = variability;
    return s;
}

std::vector<float> processedSine (const ParamSnapshot& snap, double hz,
                                  uint64_t seed, int numSamples)
{
    GenerationModel model;
    model.prepare ({ kSr, 512, 1 });
    model.setSeed (seed);
    model.reset();
    juce::AudioBuffer<float> buf (1, numSamples);
    auto* d = buf.getWritePointer (0);
    for (int i = 0; i < numSamples; ++i)
        d[i] = kProbeAmp * (float) std::sin (2.0 * M_PI * hz * i / kSr);
    for (int pos = 0; pos < numSamples; pos += 512)
    {
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 1, pos,
                                       std::min (512, numSamples - pos));
        model.process (view, snap);
    }
    return { d, d + numSamples };
}

// Steady-state response (dB re input) of the optimized model at hz.
double measureModelDb (const ParamSnapshot& snap, double hz, uint64_t seed = 1)
{
    const int n = (int) kSr;
    const auto x = processedSine (snap, hz, seed, n);
    const int half = n / 2;
    return vfatest::amplitudeToDb (vfatest::goertzelAmplitude (x.data() + half, half, hz, kSr))
         - vfatest::amplitudeToDb (kProbeAmp);
}

// Steady-state response of the literal N-stage reference chain at hz.
double measureReferenceDb (Medium medium, int generations, double hz)
{
    ExactReferenceChain ref;
    ref.prepare (kSr, medium, generations);
    ref.reset();
    const int n = (int) kSr;
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i)
        x[(size_t) i] = kProbeAmp * (float) std::sin (2.0 * M_PI * hz * i / kSr);
    ref.process (x.data(), n);
    const int half = n / 2;
    return vfatest::amplitudeToDb (vfatest::goertzelAmplitude (x.data() + half, half, hz, kSr))
         - vfatest::amplitudeToDb (kProbeAmp);
}
} // namespace

VFA_TEST (Generation_monotonic_hf_loss)
{
    const float gens[] = { 0.0f, 1.0f, 2.0f, 4.0f, 6.0f, 8.0f };
    double prev8k = 1.0e9, first8k = 0.0, last8k = 0.0;
    for (size_t k = 0; k < 6; ++k)
    {
        const auto snap = genSnap (gens[k], true, 0.0f);
        const double r8k = measureModelDb (snap, 8000.0);
        const double r1k = measureModelDb (snap, 1000.0);
        char msg[128];
        std::snprintf (msg, sizeof (msg), "N=%g: 8 kHz %.2f dB, 1 kHz %.2f dB",
                       gens[k], r8k, r1k);
        CHECK_MSG (r8k <= prev8k + 1.0e-3, msg);       // non-increasing in N
        CHECK_MSG (std::abs (r1k) <= 1.0, msg);        // mids stay near unity
        prev8k = r8k;
        if (k == 0) first8k = r8k;
        if (k == 5) last8k = r8k;
    }
    CHECK_MSG (last8k <= first8k - 6.0, "N=8 must lose >= 6 dB at 8 kHz vs N=0");
}

VFA_TEST (Generation_fractional_continuity)
{
    double prev = 0.0;
    for (int k = 0; k <= 10; ++k)
    {
        const float gens = 2.0f + 0.1f * (float) k;
        const double r6k = measureModelDb (genSnap (gens, false, 0.0f), 6000.0);
        if (k > 0)
        {
            char msg[128];
            std::snprintf (msg, sizeof (msg), "N=%.1f step %.3f dB", gens, r6k - prev);
            CHECK_MSG (std::abs (r6k - prev) <= 0.8, msg);
        }
        prev = r6k;
    }
}

VFA_TEST (Generation_noise_and_drive_helpers_monotonic)
{
    // driveLiftDb is soft-capped at 6 dB (DSP_SPEC §6): strict increase is
    // asserted below the cap, non-decreasing at/above it.
    float prevNoise = -1.0f, prevDrive = -1.0f;
    for (int gens = 0; gens <= 8; ++gens)
    {
        const auto snap = genSnap ((float) gens, true, 0.0f);
        const float noiseDb = GenerationModel::noiseLiftDb (snap);
        const float driveDb = GenerationModel::driveLiftDb (snap);
        if (gens > 0)
        {
            CHECK (noiseDb > prevNoise);
            if (prevDrive < 6.0f - 1.0e-4f)
                CHECK (driveDb > prevDrive);
            else
                CHECK (driveDb >= prevDrive - 1.0e-6f);
        }
        CHECK (driveDb <= 6.0f + 1.0e-6f);
        prevNoise = noiseDb;
        prevDrive = driveDb;
    }
    CHECK_NEAR (prevDrive, 6.0f, 1.0e-4);
    const float ws = GenerationModel::wowFlutterScale (genSnap (4.0f, true, 0.0f));
    CHECK_NEAR (ws, std::sqrt (5.0f), 0.05 * std::sqrt (5.0));
}

VFA_TEST (Generation_equivalence_with_exact_reference)
{
    // Bounds used (documented): |optimized - exact| <= 1.5 dB at 1 & 4 kHz,
    // <= 3 dB at 8 kHz, for N in {1, 2, 4, 8} on magnetic film — plus both
    // models strictly monotonic vs N at 8 kHz.
    const int gens[] = { 1, 2, 4, 8 };
    const double freqs[] = { 1000.0, 4000.0, 8000.0 };
    const double tols[]  = { 1.5, 1.5, 3.0 };
    double prevOpt8k = 1.0e9, prevRef8k = 1.0e9;
    for (int n : gens)
    {
        const auto snap = genSnap ((float) n, true, 0.0f);
        for (int f = 0; f < 3; ++f)
        {
            const double opt = measureModelDb (snap, freqs[f]);
            const double ref = measureReferenceDb (Medium::magneticFilm, n, freqs[f]);
            char msg[160];
            std::snprintf (msg, sizeof (msg), "N=%d %g Hz: optimized %.2f dB vs exact %.2f dB",
                           n, freqs[f], opt, ref);
            CHECK_MSG (std::abs (opt - ref) <= tols[f], msg);
            if (f == 2)
            {
                CHECK_MSG (opt < prevOpt8k - 0.05, msg);
                CHECK_MSG (ref < prevRef8k - 0.05, msg);
                prevOpt8k = opt;
                prevRef8k = ref;
            }
        }
    }
}

VFA_TEST (Generation_integer_mode_snaps)
{
    for (double hz : { 6000.0, 8000.0 })
    {
        const double snapped = measureModelDb (genSnap (3.4f, true, 0.5f), hz, 7);
        const double integer = measureModelDb (genSnap (3.0f, true, 0.5f), hz, 7);
        CHECK_NEAR (snapped, integer, 0.05);
    }
}

VFA_TEST (Generation_variability_deterministic)
{
    const auto snap = genSnap (3.0f, true, 1.0f);
    const int n = 24000;
    const auto a = processedSine (snap, 6000.0, 11, n);
    const auto b = processedSine (snap, 6000.0, 11, n);
    const auto c = processedSine (snap, 6000.0, 12, n);
    CHECK (std::memcmp (a.data(), b.data(), sizeof (float) * a.size()) == 0);
    CHECK_MSG (std::memcmp (a.data(), c.data(), sizeof (float) * a.size()) != 0,
               "different seeds must change the variability tilt");
}
