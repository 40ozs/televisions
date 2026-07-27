// Accumulated transfer-generation model (DSP_SPEC §6, ADR-007).
//
// process() applies the *accumulated equivalent* of N sequential transfers:
//   - HF loss: two cascaded one-poles whose combined -3 dB point tracks
//     accumulatedCutoffHz (fractional N moves it continuously, no clicks),
//     plus a small peaking tilt correction keeping 1 kHz within +-0.5 dB.
//   - Transient softening: envelope-driven attack rounding, amount 0.08*N
//     (capped 0.5) via the shared TransientSoftener primitive.
//   - Variability: a seeded +-1.5 dB high-shelf @ 3 kHz, a deterministic
//     function of (seed, round(N)) scaled by genVariability.
// Noise / drive / wow-flutter accumulation is *published* via the static
// helpers and applied by the owning modules (see Engine.cpp).
// Validated against ExactReferenceChain (test-only literal N-stage chain).
#include "GenerationModel.h"
#include "../Core/MacroMath.h"

namespace vfa::dsp
{

namespace
{
constexpr uint32_t kGenerationModuleId = 2;  // ADR-016 stream namespace

// Two identical one-poles cascade to -3 dB at fc when each pole sits at
// fc / sqrt(sqrt(2) - 1) ~= 1.5538 * fc.
constexpr float kPolePairFactor = 1.55377f;

float onePoleCoeff (float hz, double sampleRate) noexcept
{
    const float c = std::clamp (hz, 1.0f, (float) sampleRate * 0.49f);
    return 1.0f - std::exp (-2.0f * kPi * c / (float) sampleRate);
}

// Magnitude (dB) of the exp-form one-pole y += a*(x - y) at hz.
float onePoleMagnitudeDb (float a, float hz, double sampleRate) noexcept
{
    const float w = 2.0f * kPi * hz / (float) sampleRate;
    const float r = 1.0f - a;
    const float re = 1.0f - r * std::cos (w);
    const float im = r * std::sin (w);
    return gainToDb (a / std::sqrt (re * re + im * im));
}

// Coefficients only: keeps the running state so live updates stay click-free.
void copyCoefficients (const Biquad& from, Biquad& to) noexcept
{
    to.b0 = from.b0; to.b1 = from.b1; to.b2 = from.b2;
    to.a1 = from.a1; to.a2 = from.a2;
}
} // namespace

float GenerationModel::effectiveGenerations (const ParamSnapshot& snap) noexcept
{
    const float g = std::clamp (snap.generations, 0.0f, 8.0f);
    return snap.genInteger ? std::round (g) : g;
}

float GenerationModel::prototypeCutoffHz (Medium m) noexcept
{
    switch (m)
    {
        case Medium::opticalMono:
        case Medium::kinescope:      return 9000.0f;
        case Medium::opticalStereo:  return 11000.0f;
        case Medium::magneticFilm:   return 16000.0f;
        case Medium::fieldTape:      return 14000.0f;
        case Medium::consumer:       return 10000.0f;
        case Medium::broadcastMono:  return 8000.0f;
        case Medium::broadcastStereo:return 14000.0f;
        case Medium::clean:          return 20000.0f;
    }
    return 16000.0f;
}

float GenerationModel::accumulatedCutoffHz (Medium m, float generations) noexcept
{
    // N cascaded analog one-poles at f0 have their -3 dB point at
    // f0 * sqrt(2^(1/N) - 1). Documented refinement: exponent 0.55 instead of
    // 0.5 deepens the accumulated loss slightly to account for the exp-form
    // digital one-pole rolling off more gently than its analog prototype
    // near Nyquist. Keeps N=1 -> f0 and stays strictly monotonic in N
    // (validated by GenerationTest against ExactReferenceChain).
    const float f0 = prototypeCutoffHz (m);
    if (generations < 0.01f) return f0 * 4.0f;
    return f0 * std::pow (std::pow (2.0f, 1.0f / generations) - 1.0f, 0.55f);
}

float GenerationModel::noiseLiftDb (const ParamSnapshot& snap) noexcept
{
    return 3.0f * std::sqrt (effectiveGenerations (snap));
}

float GenerationModel::driveLiftDb (const ParamSnapshot& snap) noexcept
{
    return std::min (1.0f * effectiveGenerations (snap), 6.0f);
}

float GenerationModel::wowFlutterScale (const ParamSnapshot& snap) noexcept
{
    return macros::generationWowFlutterScale (effectiveGenerations (snap));
}

void GenerationModel::prepare (const StreamSpec& spec)
{
    streamSpec = spec;
    channels.assign ((size_t) spec.numChannels, {});
    for (auto& c : channels)
        c.softener.prepare (spec.sampleRate);
    variabilityTiltDb = 0.0f;
    lastGenApplied = -1.0f;
    lastMedium = Medium::clean;
    lastVariabilityGen = -1;
    lastVariabilityAmt = -1.0f;
    seedDirty = true;
}

void GenerationModel::reset()
{
    for (auto& c : channels)
    {
        c.hf1.reset();
        c.hf2.reset();
        c.tiltCorrect.reset();
        c.variabilityTilt.reset();
        c.softener.reset();
    }
}

void GenerationModel::setSeed (uint64_t s)
{
    seed = s == 0 ? 1 : s;
    seedDirty = true;
}

void GenerationModel::updateCoefficients (const ParamSnapshot& snap, float gens) noexcept
{
    const double sr = streamSpec.sampleRate;

    // Accumulated HF loss: recomputed every block (cheap) so fractional N
    // under automation moves the cutoff continuously — no clicks.
    const float cutoff = accumulatedCutoffHz (snap.medium, gens);
    const float a = onePoleCoeff (cutoff * kPolePairFactor, sr);
    for (auto& c : channels)
    {
        c.hf1.setCoefficient (a);
        c.hf2.setCoefficient (a);
    }

    // Mid tilt correction: rebuilt only on material N / medium change.
    if (std::abs (gens - lastGenApplied) > 0.005f || snap.medium != lastMedium)
    {
        lastGenApplied = gens;
        lastMedium = snap.medium;
        const float droopDb = std::clamp (-2.0f * onePoleMagnitudeDb (a, 1000.0f, sr),
                                          0.0f, 3.0f);
        Biquad proto;
        if (droopDb > 0.02f)
            proto.peak (sr, 1000.0f, 0.6f, droopDb);   // keeps 1 kHz within +-0.5 dB
        for (auto& c : channels)
            copyCoefficients (proto, c.tiltCorrect);
    }

    // Seeded variability tilt: deterministic in (seed, round(N)), scaled by
    // genVariability; rebuilt only when one of those changes materially.
    const int genIdx = (int) std::lround (gens);
    if (genIdx != lastVariabilityGen || seedDirty
        || std::abs (snap.genVariability - lastVariabilityAmt) > 1.0e-4f)
    {
        lastVariabilityGen = genIdx;
        lastVariabilityAmt = snap.genVariability;
        seedDirty = false;
        Rng vr (Rng::streamSeed (seed, kGenerationModuleId, 0, (uint32_t) genIdx));
        variabilityTiltDb = 1.5f * std::clamp (snap.genVariability, 0.0f, 1.0f)
                          * vr.nextBipolar();
        Biquad proto;
        if (std::abs (variabilityTiltDb) > 0.01f)
            proto.highShelf (sr, 3000.0f, 1.0f, variabilityTiltDb);
        for (auto& c : channels)
            copyCoefficients (proto, c.variabilityTilt);
    }
}

void GenerationModel::process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap)
{
    const int numCh = std::min (buffer.getNumChannels(), (int) channels.size());
    const int n = buffer.getNumSamples();
    if (numCh <= 0 || n <= 0)
        return;

    const float gens = effectiveGenerations (snap);
    if (gens <= 0.005f)
        return;                                  // N = 0: fully transparent

    updateCoefficients (snap, gens);

    const float softenAmount = std::min (0.08f * gens, 0.5f);
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& c = channels[(size_t) ch];
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            float v = c.hf2.process (c.hf1.process (d[i]));
            v = c.tiltCorrect.process (v);
            v = c.variabilityTilt.process (v);
            d[i] = c.softener.process (v, softenAmount);
        }
    }
}

} // namespace vfa::dsp
