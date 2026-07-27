#pragma once

#include <cmath>
#include <algorithm>

// Small deterministic filter/utility primitives shared by all modules.
// Own implementations (rather than juce::dsp::IIR) so state layout, reset
// semantics, and per-sample cost are explicit and identical everywhere.

namespace vfa::dsp
{

constexpr float kPi = 3.14159265358979323846f;

inline float dbToGain (float dB) noexcept { return std::pow (10.0f, dB * 0.05f); }
inline float gainToDb (float g)  noexcept { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

// Flush denormals/garbage to zero.
inline float flushToZero (float v) noexcept
{
    return std::abs (v) < 1.0e-25f ? 0.0f : v;
}

// Fast, bounded tanh-style saturator (rational Pade-like approximation).
// Monotonic, odd, |out| < 1. Deterministic across platforms.
inline float fastTanh (float x) noexcept
{
    x = std::clamp (x, -5.0f, 5.0f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// ----------------------------------------------------------------- one-pole
struct OnePoleLP
{
    void setCutoff (float hz, double sampleRate) noexcept
    {
        const float c = std::clamp (hz, 1.0f, float (sampleRate) * 0.49f);
        a = 1.0f - std::exp (-2.0f * kPi * c / float (sampleRate));
    }
    void setCoefficient (float coeff) noexcept { a = std::clamp (coeff, 0.0f, 1.0f); }
    float process (float x) noexcept { z += a * (x - z); z = flushToZero (z); return z; }
    void reset() noexcept { z = 0.0f; }
    float state() const noexcept { return z; }

    float a = 1.0f, z = 0.0f;
};

struct OnePoleHP
{
    void setCutoff (float hz, double sampleRate) noexcept { lp.setCutoff (hz, sampleRate); }
    float process (float x) noexcept { return x - lp.process (x); }
    void reset() noexcept { lp.reset(); }
    OnePoleLP lp;
};

// ------------------------------------------------------------------- biquad
// Transposed direct form II, RBJ designs.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = flushToZero (b1 * x - a1 * y + z2);
        z2 = flushToZero (b2 * x - a2 * y);
        return y;
    }
    void reset() noexcept { z1 = z2 = 0.0f; }

    void setIdentity() noexcept { b0 = 1; b1 = b2 = a1 = a2 = 0; }

    void lowpass (double sr, float hz, float q) noexcept
    {
        const float w = 2.0f * kPi * clampFreq (hz, sr) / float (sr);
        const float cw = std::cos (w), sw = std::sin (w), alpha = sw / (2.0f * q);
        const float a0 = 1.0f + alpha;
        b0 = (1.0f - cw) * 0.5f / a0; b1 = (1.0f - cw) / a0; b2 = b0;
        a1 = -2.0f * cw / a0; a2 = (1.0f - alpha) / a0;
    }
    void highpass (double sr, float hz, float q) noexcept
    {
        const float w = 2.0f * kPi * clampFreq (hz, sr) / float (sr);
        const float cw = std::cos (w), sw = std::sin (w), alpha = sw / (2.0f * q);
        const float a0 = 1.0f + alpha;
        b0 = (1.0f + cw) * 0.5f / a0; b1 = -(1.0f + cw) / a0; b2 = b0;
        a1 = -2.0f * cw / a0; a2 = (1.0f - alpha) / a0;
    }
    void bandpass (double sr, float hz, float q) noexcept
    {
        const float w = 2.0f * kPi * clampFreq (hz, sr) / float (sr);
        const float cw = std::cos (w), sw = std::sin (w), alpha = sw / (2.0f * q);
        const float a0 = 1.0f + alpha;
        b0 = alpha / a0; b1 = 0.0f; b2 = -alpha / a0;
        a1 = -2.0f * cw / a0; a2 = (1.0f - alpha) / a0;
    }
    void peak (double sr, float hz, float q, float gainDb) noexcept
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 2.0f * kPi * clampFreq (hz, sr) / float (sr);
        const float cw = std::cos (w), sw = std::sin (w), alpha = sw / (2.0f * q);
        const float a0 = 1.0f + alpha / A;
        b0 = (1.0f + alpha * A) / a0; b1 = -2.0f * cw / a0; b2 = (1.0f - alpha * A) / a0;
        a1 = b1; a2 = (1.0f - alpha / A) / a0;
    }
    void lowShelf (double sr, float hz, float slope, float gainDb) noexcept
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 2.0f * kPi * clampFreq (hz, sr) / float (sr);
        const float cw = std::cos (w), sw = std::sin (w);
        const float alpha = sw * 0.5f * std::sqrt ((A + 1.0f / A) * (1.0f / slope - 1.0f) + 2.0f);
        const float k = 2.0f * std::sqrt (A) * alpha;
        const float a0 = (A + 1) + (A - 1) * cw + k;
        b0 = A * ((A + 1) - (A - 1) * cw + k) / a0;
        b1 = 2 * A * ((A - 1) - (A + 1) * cw) / a0;
        b2 = A * ((A + 1) - (A - 1) * cw - k) / a0;
        a1 = -2 * ((A - 1) + (A + 1) * cw) / a0;
        a2 = ((A + 1) + (A - 1) * cw - k) / a0;
    }
    void highShelf (double sr, float hz, float slope, float gainDb) noexcept
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = 2.0f * kPi * clampFreq (hz, sr) / float (sr);
        const float cw = std::cos (w), sw = std::sin (w);
        const float alpha = sw * 0.5f * std::sqrt ((A + 1.0f / A) * (1.0f / slope - 1.0f) + 2.0f);
        const float k = 2.0f * std::sqrt (A) * alpha;
        const float a0 = (A + 1) - (A - 1) * cw + k;
        b0 = A * ((A + 1) + (A - 1) * cw + k) / a0;
        b1 = -2 * A * ((A - 1) + (A + 1) * cw) / a0;
        b2 = A * ((A + 1) + (A - 1) * cw - k) / a0;
        a1 = 2 * ((A - 1) - (A + 1) * cw) / a0;
        a2 = ((A + 1) - (A - 1) * cw - k) / a0;
    }

private:
    static float clampFreq (float hz, double sr) noexcept
    {
        return std::clamp (hz, 1.0f, float (sr) * 0.49f);
    }
};

// -------------------------------------------------------------- DC blocking
struct DcBlocker
{
    void prepare (double sampleRate, float cutoffHz = 5.0f) noexcept
    {
        r = 1.0f - 2.0f * kPi * cutoffHz / float (sampleRate);
        r = std::clamp (r, 0.9f, 0.999999f);
    }
    float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x; y1 = flushToZero (y);
        return y;
    }
    void reset() noexcept { x1 = y1 = 0.0f; }
    float r = 0.995f, x1 = 0.0f, y1 = 0.0f;
};

// ------------------------------------------------------- envelope follower
struct EnvelopeFollower
{
    void prepare (double sampleRate, float attackMs, float releaseMs) noexcept
    {
        sr = sampleRate;
        setTimes (attackMs, releaseMs);
    }
    void setTimes (float attackMs, float releaseMs) noexcept
    {
        aAtt = coeff (attackMs);
        aRel = coeff (releaseMs);
    }
    float process (float x) noexcept
    {
        const float ax = std::abs (x);
        env += (ax > env ? aAtt : aRel) * (ax - env);
        env = flushToZero (env);
        return env;
    }
    void reset() noexcept { env = 0.0f; }
    float value() const noexcept { return env; }

private:
    float coeff (float ms) const noexcept
    {
        const float t = std::max (ms, 0.01f) * 0.001f;
        return 1.0f - std::exp (-1.0f / (t * float (sr)));
    }
    double sr = 48000.0;
    float aAtt = 1.0f, aRel = 0.1f, env = 0.0f;
};

// ------------------------------------------------------ transient softener
// Envelope-driven variable one-pole: cutoff drops while the signal attack is
// rising, rounding transients the way slow media/electronics do. amount 0..1.
struct TransientSoftener
{
    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate;
        fastEnv.prepare (sampleRate, 0.5f, 30.0f);
        slowEnv.prepare (sampleRate, 8.0f, 80.0f);
        lp.setCutoff (20000.0f, sampleRate);
    }
    float process (float x, float amount) noexcept
    {
        const float fast = fastEnv.process (x);
        const float slow = slowEnv.process (x);
        // Attack detected when the fast envelope exceeds the slow one.
        const float attack = std::max (0.0f, fast - slow) / (slow + 1.0e-4f);
        const float atten = std::clamp (attack * amount * 2.0f, 0.0f, 1.0f);
        const float cutoff = 18000.0f * std::pow (0.12f, atten); // down to ~2.2 kHz
        lp.setCutoff (cutoff, sr);
        return lp.process (x);
    }
    void reset() noexcept { fastEnv.reset(); slowEnv.reset(); lp.reset(); }

    double sr = 48000.0;
    EnvelopeFollower fastEnv, slowEnv;
    OnePoleLP lp;
};

} // namespace vfa::dsp
