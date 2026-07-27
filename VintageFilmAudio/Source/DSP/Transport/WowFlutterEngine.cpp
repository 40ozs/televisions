// Wow/flutter/drift/scrape speed-instability engine (DSP_SPEC §5, ADR-006).
//
// One bounded fractional-delay line per channel (cubic Hermite / Catmull-Rom)
// around a fixed centre delay. The modulator is generated once per
// controlInterval samples and linearly interpolated per sample (no zipper);
// the summed modulation is hard-clamped to +-maxExcursionMs by construction
// AND as a final clamp — it can never exceed the bound.
//
// Speed-to-delay conversion: for a sinusoidal delay d(t) = D*sin(2*pi*f*t)
// the instantaneous speed deviation is d'(t), peaking at 2*pi*f*D. A target
// peak speed-deviation fraction s at rate f therefore maps to a delay
// excursion D = s / (2*pi*f) seconds.
#include "WowFlutterEngine.h"

namespace vfa::dsp
{

namespace
{
constexpr uint32_t kTransportModuleId = 1;   // ADR-016 stream namespace
constexpr float twoPi = 2.0f * kPi;
constexpr float sigmaUniform = 0.5773503f;   // RMS of Rng::nextBipolar()

// TransportQuality multiplies every modulation depth (DSP_SPEC §5).
float qualityDepthScale (TransportQuality q) noexcept
{
    switch (q)
    {
        case TransportQuality::lab:      return 0.25f;
        case TransportQuality::studio:   return 1.0f;
        case TransportQuality::portable: return 1.8f;
        case TransportQuality::worn:     return 3.0f;
        case TransportQuality::damaged:  return 5.0f;
    }
    return 1.0f;
}

// Delay excursion (ms) of a unit peak speed-deviation fraction at rateHz.
float speedFractionToMs (float fraction, float rateHz) noexcept
{
    return 1000.0f * fraction / (twoPi * std::max (rateHz, 0.01f));
}

// One-pole coefficient at the control rate. Computed directly (not via
// OnePoleLP::setCutoff, which clamps to >= 1 Hz) so sub-Hz drift/jitter
// cutoffs are representable.
float controlCoeff (float hz, float controlRateHz) noexcept
{
    return 1.0f - std::exp (-twoPi * hz / controlRateHz);
}

// 1 / RMS of white nextBipolar() noise through a one-pole LP with coeff a.
float onePoleNoiseNorm (float a) noexcept
{
    return 1.0f / (sigmaUniform * std::sqrt (a / (2.0f - a)));
}

// 1 / RMS of white nextBipolar() noise through the 0 dB-peak-gain bandpass
// (equivalent noise bandwidth ~= 0.785 * f0/Q for the 2nd-order section).
float bandpassNoiseNorm (float f0, float q, float controlRateHz) noexcept
{
    const float enbw = 0.785f * f0 / q;
    return 1.0f / (sigmaUniform
                   * std::sqrt (std::min (enbw / (0.5f * controlRateHz), 1.0f)));
}

float wrapPhase (float p) noexcept
{
    if (p >= twoPi)
        p -= twoPi;
    return p;
}
} // namespace

float WowFlutterEngine::depthToSpeedFraction (float p) noexcept
{
    const float c = std::clamp (p, 0.0f, 1.0f);
    return 0.004f * c * c; // 0.5 -> 0.1 %, 1.0 -> 0.4 %
}

void WowFlutterEngine::prepare (const StreamSpec& spec)
{
    streamSpec = spec;
    centreDelaySamples = centreDelayMs * 0.001f * (float) spec.sampleRate;
    delayLineLength = (int) std::ceil ((centreDelayMs + maxExcursionMs + 4.0f) * 0.001 * spec.sampleRate);
    controlRateHz = (float) spec.sampleRate / (float) controlInterval;
    // Depth changes smooth over ~50 ms, applied at control rate.
    depthSmoothCoeff = 1.0f - std::exp (-(float) controlInterval
                                        / (0.050f * (float) spec.sampleRate));
    driftNorm     = onePoleNoiseNorm (controlCoeff (0.08f, controlRateHz));
    jitterNorm    = onePoleNoiseNorm (controlCoeff (0.15f, controlRateHz));
    ampJitterNorm = onePoleNoiseNorm (controlCoeff (0.40f, controlRateHz));
    scrapeNorm    = bandpassNoiseNorm (110.0f, 0.8f, controlRateHz);

    channels.assign ((size_t) spec.numChannels, {});
    for (auto& c : channels)
    {
        c.delayLine.assign ((size_t) delayLineLength, 0.0f);
        configureModulator (c.mod);
    }
    configureModulator (linkedMod);
    setSeed (baseSeed);
    reset();
}

void WowFlutterEngine::configureModulator (ModulatorState& m) noexcept
{
    // Drift: bounded random walk = leaky-integrated white noise (a one-pole
    // LP is exactly a leaky integrator), spectrum bounded <= 0.15 Hz.
    m.driftLp1.setCoefficient (controlCoeff (0.08f, controlRateHz));
    m.driftLp2.setCoefficient (controlCoeff (0.15f, controlRateHz));
    m.rateJitterLp.setCoefficient (controlCoeff (0.15f, controlRateHz));
    m.ampJitterLp.setCoefficient (controlCoeff (0.40f, controlRateHz));
    m.scrapeBp.bandpass (controlRateHz, 110.0f, 0.8f);   // ~60-200 Hz band
    m.flutterHzApplied = -1.0f;                          // (re)designed on first tick
}

void WowFlutterEngine::reset()
{
    for (auto& c : channels)
    {
        std::fill (c.delayLine.begin(), c.delayLine.end(), 0.0f);
        c.writePos = 0;
        resetModulator (c.mod);
    }
    resetModulator (linkedMod);
    controlCountdown = 0;
}

void WowFlutterEngine::resetModulator (ModulatorState& m) noexcept
{
    // Dynamic state only; the Rng streams are (re)wound by setSeed (ADR-016),
    // so setSeed + reset is bit-exact reproducible.
    m.wowPhase1 = m.wowPhase2 = m.motorPhase = 0.0f;
    m.driftLp1.reset();
    m.driftLp2.reset();
    m.rateJitterLp.reset();
    m.ampJitterLp.reset();
    m.flutterBp.reset();
    m.scrapeBp.reset();
    m.flutterHzApplied = -1.0f;
    m.wowDepthSm = m.flutterDepthSm = m.driftDepthSm = m.scrapeDepthSm = 0.0f;
    m.lastValueMs = m.nextValueMs = 0.0f;
}

void WowFlutterEngine::setSeed (uint64_t s)
{
    baseSeed = s == 0 ? 1 : s;
    for (size_t ch = 0; ch < channels.size(); ++ch)
        reseedModulator (channels[ch].mod, (uint32_t) ch);
    reseedModulator (linkedMod, 0);      // linked mode shares channel-0 streams
}

void WowFlutterEngine::reseedModulator (ModulatorState& m, uint32_t channel) noexcept
{
    m.driftRng.seed   (Rng::streamSeed (baseSeed, kTransportModuleId, channel, purposeDrift));
    m.jitterRng.seed  (Rng::streamSeed (baseSeed, kTransportModuleId, channel, purposeWowJitter));
    m.flutterRng.seed (Rng::streamSeed (baseSeed, kTransportModuleId, channel, purposeFlutterNoise));
    m.scrapeRng.seed  (Rng::streamSeed (baseSeed, kTransportModuleId, channel, purposeScrape));
}

float WowFlutterEngine::computeControlValueMs (ModulatorState& m, const ParamSnapshot& snap,
                                               float depthScale) noexcept
{
    const float fsc = controlRateHz;
    const float quality = qualityDepthScale (snap.transportQuality);
    const float accum = quality * std::max (depthScale, 0.0f);

    // Depth targets, smoothed ~50 ms at control rate. wow/flutter/drift are
    // peak speed-deviation fractions; scrape is specified directly in ms
    // (HIST-APPROX). depthScale (generation accumulation) multiplies
    // wow + flutter + drift; scrape only gates on it (clamped to <= 1) so
    // depthScale 0 still means fixed-centre-delay only.
    const float wowT     = depthToSpeedFraction (snap.wow) * accum;
    const float flutterT = depthToSpeedFraction (snap.flutter) * 0.35f * accum;
    const float driftT   = depthToSpeedFraction (snap.drift) * accum;
    const float scrapeT  = std::clamp (snap.scrape, 0.0f, 1.0f) * 0.02f * quality
                         * std::clamp (depthScale, 0.0f, 1.0f);
    m.wowDepthSm     += depthSmoothCoeff * (wowT     - m.wowDepthSm);
    m.flutterDepthSm += depthSmoothCoeff * (flutterT - m.flutterDepthSm);
    m.driftDepthSm   += depthSmoothCoeff * (driftT   - m.driftDepthSm);
    m.scrapeDepthSm  += depthSmoothCoeff * (scrapeT  - m.scrapeDepthSm);

    // Fixed draw order and count per tick: the stream position stays aligned
    // regardless of parameter values (determinism under automation).
    const float rateNoise    = m.jitterRng.nextBipolar();
    const float ampNoise     = m.jitterRng.nextBipolar();
    const float driftNoise   = m.driftRng.nextBipolar();
    const float flutterNoise = m.flutterRng.nextBipolar();
    const float scrapeNoise  = m.scrapeRng.nextBipolar();

    // Drift: bounded random walk through leaky integrators (rate <= 0.15 Hz),
    // expressed as a 0.1 Hz-equivalent speed deviation.
    const float driftUnit = std::clamp (m.driftLp2.process (m.driftLp1.process (driftNoise)
                                                            * driftNorm),
                                        -2.5f, 2.5f);
    const float driftMs = m.driftDepthSm * speedFractionToMs (1.0f, 0.1f) * 0.5f * driftUnit;

    // Wow: two quasi-periodic components (rate and ~1.9x rate at 40 % weight,
    // normalized 0.714/0.286 so their aligned sum hits the calibrated depth),
    // with slow +-15 % rate jitter (anti-correlated between the components)
    // and +-25 % amplitude jitter, both from filtered noise.
    const float rj = 0.15f * std::clamp (m.rateJitterLp.process (rateNoise) * jitterNorm,
                                         -1.0f, 1.0f);
    const float aj = 0.25f * std::clamp (m.ampJitterLp.process (ampNoise) * ampJitterNorm,
                                         -1.0f, 1.0f);
    const float wowRate = std::max (snap.wowRateHz, 0.05f);
    m.wowPhase1 = wrapPhase (m.wowPhase1 + twoPi * wowRate * (1.0f + rj) / fsc);
    m.wowPhase2 = wrapPhase (m.wowPhase2 + twoPi * 1.9f * wowRate * (1.0f - rj) / fsc);
    const float wowMs = m.wowDepthSm * (1.0f + aj)
                      * (0.714f * speedFractionToMs (1.0f, wowRate) * std::sin (m.wowPhase1)
                       + 0.286f * speedFractionToMs (1.0f, 1.9f * wowRate) * std::sin (m.wowPhase2));

    // Flutter: narrowband (~1/3 octave, Q ~= 4.32) filtered noise centred on
    // the flutter rate + one deterministic motor component at 0.8x rate.
    const float flutterHz = std::clamp (snap.flutterRateHz, 4.0f, 0.45f * fsc);
    if (std::abs (flutterHz - m.flutterHzApplied) > 0.01f * flutterHz)
    {
        m.flutterHzApplied = flutterHz;
        m.flutterBp.bandpass (fsc, flutterHz, 4.32f);
        m.flutterNorm = bandpassNoiseNorm (flutterHz, 4.32f, fsc);
    }
    const float flutterUnit = std::clamp (m.flutterBp.process (flutterNoise) * m.flutterNorm,
                                          -3.0f, 3.0f);
    const float motorHz = 0.8f * flutterHz;
    m.motorPhase = wrapPhase (m.motorPhase + twoPi * motorHz / fsc);
    const float flutterMs = m.flutterDepthSm
                          * (0.5f * speedFractionToMs (1.0f, flutterHz) * flutterUnit
                           + 0.4f * speedFractionToMs (1.0f, motorHz) * std::sin (m.motorPhase));

    // Scrape: 60-200 Hz filtered-noise FM at small depth (HIST-APPROX).
    const float scrapeUnit = std::clamp (m.scrapeBp.process (scrapeNoise) * scrapeNorm,
                                         -2.5f, 2.5f);
    const float scrapeMs = m.scrapeDepthSm * 0.4f * scrapeUnit;

    // Hard excursion bound (DSP_SPEC §5): the final value may NEVER exceed it.
    return std::clamp (driftMs + wowMs + flutterMs + scrapeMs,
                       -maxExcursionMs, maxExcursionMs);
}

void WowFlutterEngine::process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap,
                                float depthScale)
{
    const int numCh = std::min (buffer.getNumChannels(), (int) channels.size());
    const int n = buffer.getNumSamples();
    if (numCh <= 0 || n <= 0 || delayLineLength <= 8)
        return;

    const bool linked = snap.stereoLink;
    const float msToSamples = 0.001f * (float) streamSpec.sampleRate;
    const float invInterval = 1.0f / (float) controlInterval;
    const float lenF = (float) delayLineLength;
    const float minDelay = 4.0f;
    const float maxDelay = (float) delayLineLength - 5.0f;

    // Link-mode change: the newly selected modulator's control endpoints may
    // be stale (frozen at whatever they held when it last ran), which would
    // step the read position by up to the full excursion in one sample.
    // Hand the active endpoints across so the delay stays continuous (F5).
    if (linked != lastLinked)
    {
        if (linked)
        {
            linkedMod.lastValueMs = channels[0].mod.lastValueMs;
            linkedMod.nextValueMs = channels[0].mod.nextValueMs;
        }
        else
        {
            for (auto& c : channels)
            {
                c.mod.lastValueMs = linkedMod.lastValueMs;
                c.mod.nextValueMs = linkedMod.nextValueMs;
            }
        }
        lastLinked = linked;
    }

    for (int i = 0; i < n; ++i)
    {
        if (controlCountdown <= 0)
        {
            if (linked)
            {
                linkedMod.lastValueMs = linkedMod.nextValueMs;
                linkedMod.nextValueMs = computeControlValueMs (linkedMod, snap, depthScale);
            }
            else
            {
                for (int ch = 0; ch < numCh; ++ch)
                {
                    auto& m = channels[(size_t) ch].mod;
                    m.lastValueMs = m.nextValueMs;
                    m.nextValueMs = computeControlValueMs (m, snap, depthScale);
                }
            }
            controlCountdown = controlInterval;
        }
        --controlCountdown;
        const float frac = 1.0f - (float) controlCountdown * invInterval;

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto& c = channels[(size_t) ch];
            const auto& m = linked ? linkedMod : c.mod;
            // Belt and braces: the endpoints are already clamped, so the
            // interpolated value is too — clamp anyway per spec.
            const float modMs = std::clamp (m.lastValueMs + (m.nextValueMs - m.lastValueMs) * frac,
                                            -maxExcursionMs, maxExcursionMs);
            const float delay = std::clamp (centreDelaySamples + modMs * msToSamples,
                                            minDelay, maxDelay);

            float* line = c.delayLine.data();
            auto* d = buffer.getWritePointer (ch);
            line[(size_t) c.writePos] = d[i];

            // Branch-safe fractional read with wrap (delay >= 4 keeps the
            // interpolation taps clear of the write position).
            float rp = (float) c.writePos - delay;
            if (rp < 0.0f)
                rp += lenF;
            int i1 = (int) rp;
            const float t = rp - (float) i1;
            if (i1 >= delayLineLength) i1 -= delayLineLength;
            int i0 = i1 - 1; if (i0 < 0)                i0 += delayLineLength;
            int i2 = i1 + 1; if (i2 >= delayLineLength) i2 -= delayLineLength;
            int i3 = i2 + 1; if (i3 >= delayLineLength) i3 -= delayLineLength;

            const float p0 = line[(size_t) i0], p1 = line[(size_t) i1],
                        p2 = line[(size_t) i2], p3 = line[(size_t) i3];
            // Cubic Hermite (Catmull-Rom) between p1 and p2; t = 0 returns p1
            // exactly, so zero depth is a bit-exact integer centre delay.
            const float c1 = 0.5f * (p2 - p0);
            const float c2 = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
            const float c3 = 0.5f * (p3 - p0) + 1.5f * (p1 - p2);
            d[i] = ((c3 * t + c2) * t + c1) * t + p1;

            if (++c.writePos >= delayLineLength)
                c.writePos = 0;
        }
    }
}

} // namespace vfa::dsp
