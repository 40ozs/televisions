// Noise & artifact engine (DSP_SPEC §8, ADR-008/016).
//
// All sources are generated procedurally per sample from seeded Rng streams
// (streamSeed (seed, moduleId=2, channel, purpose)) — no lookup buffers or
// loops anywhere. Events are Poisson-scheduled with seeded exponential draws.
// Continuous sources are calibrated at prepare() by measuring the RMS gain of
// each spectral chain over a fixed-seed white burst, so that param = 0.5 hits
// the SNR-table level of DSP_SPEC §3 (noise RMS = -18 dBFS - SNR) and
// param = 1.0 sits 20 dB above it (quadratic mapping below 0.5, HIST-APPROX).
//
// DEV-002: print-through implements the POST-echo only. Pre-echo would need
// lookahead latency and is a documented MVP limitation (KNOWN_LIMITATIONS.md).
#include "NoiseArtifactEngine.h"

namespace vfa::dsp
{

namespace
{
constexpr uint32_t kModuleId = 2;
constexpr uint32_t kSharedChannel = 0xFFFFu;   // width-mix / correlated streams

enum Purpose : uint32_t
{
    purposeHiss = 0, purposeCell, purposeBroadcast, purposeCrackle,
    purposeDirt, purposeDropout, purposeMicro, purposeProjector
};

constexpr float kTwoPi = 2.0f * kPi;

// Param 0..1 -> linear level. Quadratic up to 0.5 (where the source hits its
// calibrated SNR-table RMS), then a further +20 dB by 1.0 (HIST-APPROX law).
float levelCurve (float p) noexcept
{
    p = std::clamp (p, 0.0f, 1.0f);
    if (p <= 0.0f)
        return 0.0f;
    const float q = std::min (2.0f * p, 1.0f);
    return q * q * dbToGain (std::max (0.0f, 2.0f * p - 1.0f) * 20.0f);
}

// Medium SNR targets (DSP_SPEC §3). Continuous noise RMS = -18 dBFS - SNR.
// Clean is nominally infinite; 75 dB keeps the source far down but usable.
float mediumSnrDb (Medium m) noexcept
{
    switch (m)
    {
        case Medium::clean:           return 75.0f;  // HIST-APPROX stand-in for inf
        case Medium::opticalMono:     return 38.0f;
        case Medium::opticalStereo:   return 48.0f;
        case Medium::magneticFilm:    return 55.0f;
        case Medium::fieldTape:       return 52.0f;
        case Medium::kinescope:       return 35.0f;
        case Medium::broadcastMono:   return 45.0f;
        case Medium::broadcastStereo: return 55.0f;
        case Medium::consumer:        return 40.0f;
    }
    return 55.0f;
}

enum class HissSpectrum { dark, optical, broadcastMix };

HissSpectrum hissSpectrumFor (Medium m) noexcept
{
    switch (m)
    {
        case Medium::opticalMono:
        case Medium::opticalStereo:
        case Medium::kinescope:       return HissSpectrum::optical;
        case Medium::broadcastMono:
        case Medium::broadcastStereo: return HissSpectrum::broadcastMix;
        default:                      return HissSpectrum::dark;
    }
}

// Seeded exponential inter-arrival draw in "unit-rate" units; the scheduler
// subtracts rate/fs per sample, so density tracks the amount immediately.
double expDraw (Rng& r) noexcept
{
    const double u = 1.0 - (double) r.next01();          // (0, 1]
    return std::clamp (-std::log (u), 1.0e-4, 30.0);
}

float uniform (Rng& r, float lo, float hi) noexcept
{
    return lo + (hi - lo) * r.next01();
}

// Hiss darkening: white mixed with a ~3 kHz one-pole to approximate a gentle
// -3 dB/oct tilt (HIST-APPROX).
inline float darkHiss (OnePoleLP& lp, float x) noexcept
{
    return 0.35f * x + 0.65f * lp.process (x);
}

// Triangular-ish FM noise floor: first-order rise of ~6 dB/oct above 3 kHz.
inline float bcastTilt (float& prev, float k, float x) noexcept
{
    const float y = x + k * (x - prev);
    prev = flushToZero (x);
    return y;
}

// Projector/gate texture: 24 Hz raised-cosine thump train (lowpassed to
// ~60-100 Hz energy content) + 96 Hz-AM'd 500 Hz-2 kHz noise bed. Shared by
// the runtime path and the prepare-time calibration so gains match exactly.
inline float projectorSample (double& thumpPhase, double& amPhase, Biquad& bp,
                              OnePoleLP& thumpLp, Rng& rng, double sr) noexcept
{
    constexpr double thumpHz = 24.0, gateHz = 96.0;
    const float pulseFrac = float (0.006 * thumpHz);     // 6 ms pulse per cycle
    float raw = 0.0f;
    if (thumpPhase < (double) pulseFrac)
        raw = 0.5f * (1.0f - std::cos (kTwoPi * (float) thumpPhase / pulseFrac));
    raw -= 0.5f * pulseFrac;                             // remove pulse-train DC
    const float thump = thumpLp.process (raw);

    const float bed = bp.process (rng.nextBipolar());
    const float am = 0.75f + 0.25f * std::cos (kTwoPi * (float) amPhase);

    thumpPhase += thumpHz / sr; if (thumpPhase >= 1.0) thumpPhase -= 1.0;
    amPhase    += gateHz  / sr; if (amPhase    >= 1.0) amPhase    -= 1.0;
    return 1.2f * thump + 0.5f * bed * am;
}
} // namespace

// --------------------------------------------------------------- lifecycle
void NoiseArtifactEngine::prepare (const StreamSpec& spec)
{
    streamSpec = spec;
    const double sr = spec.sampleRate;

    channels.assign ((size_t) spec.numChannels, {});
    for (auto& c : channels)
    {
        c.hissDarkLp.setCutoff (3000.0f, sr);
        c.hissCellShelf.highShelf (sr, 6000.0f, 0.9f, 6.0f);
        c.cellShelf.highShelf (sr, 6000.0f, 0.9f, 6.0f);
        c.dropoutLp.setCutoff (1800.0f, sr);
    }

    printThrough.assign ((size_t) spec.numChannels, {});
    printDelaySamples = (int) (0.4 * sr);                // ≈ one wrap, HIST-APPROX
    for (auto& p : printThrough)
    {
        p.delay.assign ((size_t) printDelaySamples + 1, 0.0f);
        p.lp.setCutoff (4000.0f, sr);
    }

    hum = {};
    hum.projBp.bandpass (sr, 1000.0f, 0.8f);
    hum.projThumpLp.setCutoff (100.0f, sr);

    programEnv.prepare (sr, 5.0f, 200.0f);

    const float smoothCoeff = 1.0f - std::exp (-1.0f / (0.05f * (float) sr));
    for (auto* s : { &smHiss, &smCell, &smBcast, &smHum, &smBuzz,
                     &smProj, &smPrint, &smDuck, &smWidth })
        s->setCoefficient (smoothCoeff);

    bcastDiffK = float (sr / (2.0 * kPi * 3000.0));

    // ---- calibration: measured 1/RMS of each spectral chain over a 1 s
    // fixed-seed white burst (independent of the user seed; prepare-only).
    const int n = std::max (4096, (int) sr);
    {
        Rng r (0x1001); OnePoleLP lp; lp.setCutoff (3000.0f, sr);
        double acc = 0;
        for (int i = 0; i < n; ++i)
        { const float y = darkHiss (lp, r.nextBipolar()); acc += (double) y * y; }
        cal.hissDark = float (1.0 / std::sqrt (acc / n));
    }
    {
        Rng r (0x1002); Biquad sh; sh.highShelf (sr, 6000.0f, 0.9f, 6.0f);
        double acc = 0;
        for (int i = 0; i < n; ++i)
        { const float y = sh.process (r.nextBipolar()); acc += (double) y * y; }
        cal.hissOptical = float (1.0 / std::sqrt (acc / n));
    }
    {
        Rng r (0x1003); OnePoleLP lp; lp.setCutoff (3000.0f, sr); float prev = 0;
        double acc = 0;
        for (int i = 0; i < n; ++i)
        {
            const float x = r.nextBipolar();
            const float y = 0.70710678f * (darkHiss (lp, x) + bcastTilt (prev, bcastDiffK, x));
            acc += (double) y * y;
        }
        cal.hissBroadcast = float (1.0 / std::sqrt (acc / n));
    }
    {
        Rng r (0x1004); Biquad sh; sh.highShelf (sr, 6000.0f, 0.9f, 6.0f);
        double acc = 0, amPhase = 0;
        for (int i = 0; i < n; ++i)
        {
            const float am = 0.9f + 0.1f * std::cos (kTwoPi * (float) amPhase);
            amPhase += 96.0 / sr; if (amPhase >= 1.0) amPhase -= 1.0;
            const float y = sh.process (r.nextBipolar()) * am;
            acc += (double) y * y;
        }
        cal.cell = float (1.0 / std::sqrt (acc / n));
    }
    {
        Rng r (0x1005); float prev = 0;
        double acc = 0;
        for (int i = 0; i < n; ++i)
        { const float y = bcastTilt (prev, bcastDiffK, r.nextBipolar()); acc += (double) y * y; }
        cal.broadcast = float (1.0 / std::sqrt (acc / n));
    }
    {
        Rng r (0x1006);
        Biquad bp; bp.bandpass (sr, 1000.0f, 0.8f);
        OnePoleLP lp; lp.setCutoff (100.0f, sr);
        double acc = 0, th = 0, am = 0;
        for (int i = 0; i < n; ++i)
        { const float y = projectorSample (th, am, bp, lp, r, sr); acc += (double) y * y; }
        cal.projector = float (1.0 / std::sqrt (acc / n));
    }

    setSeed (seed);
    resetRuntimeState();
}

void NoiseArtifactEngine::reset()
{
    // §9 determinism: after reset the engine replays bit-exactly for a seed.
    resetRuntimeState();
    reseedStreams();
}

void NoiseArtifactEngine::setSeed (uint64_t s)
{
    seed = (s == 0 ? 1 : s);
    reseedStreams();
}

void NoiseArtifactEngine::reseedStreams() noexcept
{
    for (size_t ch = 0; ch < channels.size(); ++ch)
    {
        auto& c = channels[ch];
        const auto u = (uint32_t) ch;
        c.hissRng.seed    (Rng::streamSeed (seed, kModuleId, u, purposeHiss));
        c.cellRng.seed    (Rng::streamSeed (seed, kModuleId, u, purposeCell));
        c.bcastRng.seed   (Rng::streamSeed (seed, kModuleId, u, purposeBroadcast));
        c.crackleRng.seed (Rng::streamSeed (seed, kModuleId, u, purposeCrackle));
        c.dirtRng.seed    (Rng::streamSeed (seed, kModuleId, u, purposeDirt));
        c.dropRng.seed    (Rng::streamSeed (seed, kModuleId, u, purposeDropout));
        c.microRng.seed   (Rng::streamSeed (seed, kModuleId, u, purposeMicro));
    }
    hum.projRng.seed     (Rng::streamSeed (seed, kModuleId, kSharedChannel, purposeProjector));
    hum.hissShared.seed  (Rng::streamSeed (seed, kModuleId, kSharedChannel, purposeHiss));
    hum.cellShared.seed  (Rng::streamSeed (seed, kModuleId, kSharedChannel, purposeCell));
    hum.bcastShared.seed (Rng::streamSeed (seed, kModuleId, kSharedChannel, purposeBroadcast));
}

void NoiseArtifactEngine::resetRuntimeState()
{
    for (auto& c : channels)
    {
        c.hissDarkLp.reset();
        c.hissCellShelf.reset();
        c.cellShelf.reset();
        c.dropoutLp.reset();
        c.hissBcPrev = c.bcastPrev = 0.0f;
        c.crackle = {}; c.dirt = {}; c.dropout = {}; c.micro = {};
    }
    for (auto& p : printThrough)
    {
        std::fill (p.delay.begin(), p.delay.end(), 0.0f);
        p.writePos = 0;
        p.lp.reset();
    }
    hum.phase = hum.buzzPhase = hum.buzzAmPhase = 0.0;
    hum.cellAmPhase = hum.projPhase = hum.projAmPhase = 0.0;
    hum.projBp.reset();
    hum.projThumpLp.reset();
    programEnv.reset();
    for (auto* s : { &smHiss, &smCell, &smBcast, &smHum, &smBuzz,
                     &smProj, &smPrint, &smDuck, &smWidth })
        s->reset();
    smDuck.z = 1.0f;    // no duck engaged at t = 0
}

// ------------------------------------------------------- impulsive events
float NoiseArtifactEngine::crackleSample (ChannelState& c, double rate, float topDb) noexcept
{
    auto& ev = c.crackle;
    auto& r = c.crackleRng;
    const double sr = streamSpec.sampleRate;
    if (rate > 0.0 && ev.remaining <= 0)
    {
        if (ev.unitsToNext < 0.0)
            ev.unitsToNext = expDraw (r);
        ev.unitsToNext -= rate / sr;
        if (ev.unitsToNext <= 0.0)
        {
            ev.unitsToNext = expDraw (r);
            // Short exponentially-decaying bandpass ring, per-event randomized.
            const float f = uniform (r, 1500.0f, 4000.0f);
            const float tauMs = uniform (r, 1.0f, 4.0f);
            ev.amp = dbToGain (topDb - 25.0f * r.next01());   // log-uniform
            ev.phase = 0.0f;
            ev.phaseInc = float (f / sr);
            ev.decay = std::exp (-1.0f / (tauMs * 0.001f * (float) sr));
            ev.remaining = (int) (tauMs * 0.007f * (float) sr);   // ~7 tau
        }
    }
    if (ev.remaining <= 0)
        return 0.0f;
    --ev.remaining;
    ev.phase += ev.phaseInc; if (ev.phase >= 1.0f) ev.phase -= 1.0f;
    const float v = ev.amp * std::sin (kTwoPi * ev.phase);
    ev.amp = flushToZero (ev.amp * ev.decay);
    return v;
}

float NoiseArtifactEngine::dirtSample (ChannelState& c, double rate, float topDb) noexcept
{
    auto& ev = c.dirt;
    auto& r = c.dirtRng;
    const double sr = streamSpec.sampleRate;
    if (rate > 0.0 && ev.remaining <= 0)
    {
        if (ev.unitsToNext < 0.0)
            ev.unitsToNext = expDraw (r);
        ev.unitsToNext -= rate / sr;
        if (ev.unitsToNext <= 0.0)
        {
            ev.unitsToNext = expDraw (r);
            // Larger asymmetric bipolar click, one-pole LP shaped, 2-10 ms.
            const float lenMs = uniform (r, 2.0f, 10.0f);
            ev.length = std::max (8, (int) (lenMs * 0.001f * (float) sr));
            ev.pos = 0;
            ev.remaining = ev.length;
            ev.amp = dbToGain (topDb - 20.0f * r.next01());
            ev.amp2 = -ev.amp * uniform (r, 0.3f, 0.7f);      // asymmetric lobe
            ev.lp.setCutoff (uniform (r, 800.0f, 2500.0f), sr);
            ev.lp.reset();
        }
    }
    if (ev.remaining <= 0)
        return 0.0f;
    const float t = (float) ev.pos / (float) ev.length;
    const float raw = t < 0.3f ? ev.amp
                               : ev.amp2 * (1.0f - (t - 0.3f) / 0.7f);
    ++ev.pos;
    --ev.remaining;
    return ev.lp.process (raw);
}

float NoiseArtifactEngine::microSample (ChannelState& c, double rate) noexcept
{
    auto& ev = c.micro;
    auto& r = c.microRng;
    const double sr = streamSpec.sampleRate;
    if (rate > 0.0 && ev.remaining <= 0)
    {
        if (ev.unitsToNext < 0.0)
            ev.unitsToNext = expDraw (r);
        ev.unitsToNext -= rate / sr;
        if (ev.unitsToNext <= 0.0)
        {
            ev.unitsToNext = expDraw (r);
            // Rare tube-microphonic ping: 800-1400 Hz, Q ~= 25, about -40 dB.
            const float f = uniform (r, 800.0f, 1400.0f);
            const float tau = 25.0f / (kPi * f);              // seconds, Q ~= 25
            ev.amp = dbToGain (-40.0f + uniform (r, -3.0f, 3.0f));
            ev.phase = 0.0f;
            ev.phaseInc = float (f / sr);
            ev.decay = std::exp (-1.0f / (tau * (float) sr));
            ev.remaining = (int) (7.0f * tau * (float) sr);
        }
    }
    if (ev.remaining <= 0)
        return 0.0f;
    --ev.remaining;
    ev.phase += ev.phaseInc; if (ev.phase >= 1.0f) ev.phase -= 1.0f;
    const float v = ev.amp * std::sin (kTwoPi * ev.phase);
    ev.amp = flushToZero (ev.amp * ev.decay);
    return v;
}

// Dropouts multiply the PROGRAM by a raised-cosine dip with simultaneous
// proportional HF loss — the only source that modifies instead of adds.
// Dips arrive in short clusters of 1-3 (a crease hits once per wrap;
// HIST-APPROX) so audible gaps survive statistical averaging.
float NoiseArtifactEngine::processDropout (ChannelState& c, float x, double rate, float amount) noexcept
{
    auto& ev = c.dropout;
    auto& r = c.dropRng;
    const double sr = streamSpec.sampleRate;

    if (rate > 0.0 && ev.remaining <= 0 && ev.clusterLeft <= 0)
    {
        if (ev.unitsToNext < 0.0)
            ev.unitsToNext = expDraw (r);
        ev.unitsToNext -= rate / sr;
        if (ev.unitsToNext <= 0.0)
        {
            ev.unitsToNext = expDraw (r);
            const float u = r.next01();
            ev.clusterLeft = u < 0.4f ? 1 : (u < 0.75f ? 2 : 3);
            ev.gap = 0;
        }
    }
    if (ev.remaining <= 0 && ev.clusterLeft > 0)
    {
        if (ev.gap > 0)
        {
            --ev.gap;
        }
        else
        {
            --ev.clusterLeft;
            // Depth -3..-30 dB and length 5-80 ms, biased deeper/longer as
            // the amount rises (HIST-APPROX wear law).
            const float bias = 1.0f / (1.0f + 2.0f * std::clamp (amount, 0.0f, 1.0f));
            const float lenMs = 5.0f + 75.0f * std::pow (r.next01(), bias);
            const float depthDb = 3.0f + 27.0f * std::pow (r.next01(), bias);
            ev.length = std::max (16, (int) (lenMs * 0.001f * (float) sr));
            ev.pos = 0;
            ev.remaining = ev.length;
            ev.depthLin = dbToGain (-depthDb);
            ev.primeLp = true;
        }
    }
    if (ev.remaining <= 0)
        return x;

    // Raised-cosine edges (20 % each side) around a held floor.
    const float t = (float) ev.pos / (float) ev.length;
    float w = 1.0f;
    if (t < 0.2f)      w = 0.5f * (1.0f - std::cos (kPi * t / 0.2f));
    else if (t > 0.8f) w = 0.5f * (1.0f - std::cos (kPi * (1.0f - t) / 0.2f));

    if (ev.primeLp) { c.dropoutLp.z = x; ev.primeLp = false; }
    const float lpX = c.dropoutLp.process (x);
    const float hfMix = 0.8f * w;                        // HF loss during the dip
    const float gain = 1.0f + w * (ev.depthLin - 1.0f);
    ++ev.pos;
    if (--ev.remaining == 0 && ev.clusterLeft > 0)
        ev.gap = (int) (uniform (r, 20.0f, 60.0f) * 0.001f * (float) sr);
    return ((1.0f - hfMix) * x + hfMix * lpX) * gain;
}

// ------------------------------------------------------------------ process
void NoiseArtifactEngine::process (juce::AudioBuffer<float>& buffer,
                                   const ParamSnapshot& snap, float extraNoiseDb)
{
    const int n = buffer.getNumSamples();
    const int numCh = std::min (buffer.getNumChannels(), (int) channels.size());
    if (n <= 0 || numCh <= 0)
        return;

    const double sr = streamSpec.sampleRate;
    const float extraLin = dbToGain (extraNoiseDb);      // generation floor lift

    // ---- continuous-source level targets ---------------------------------
    const auto spectrum = hissSpectrumFor (snap.medium);
    const float hissCal = spectrum == HissSpectrum::dark    ? cal.hissDark
                        : spectrum == HissSpectrum::optical ? cal.hissOptical
                                                            : cal.hissBroadcast;
    const float hissTarget = levelCurve (snap.nsHiss)
                           * dbToGain (-18.0f - mediumSnrDb (snap.medium))
                           * hissCal * extraLin;
    const float cellTarget  = levelCurve (snap.nsCell)      * dbToGain (-56.0f) * cal.cell      * extraLin;
    const float bcastTarget = levelCurve (snap.nsBroadcast) * dbToGain (-63.0f) * cal.broadcast * extraLin;
    const float projTarget  = levelCurve (snap.nsProjector) * dbToGain (-58.0f) * cal.projector;
    const float humTarget   = levelCurve (snap.nsHum)  * dbToGain (-60.0f);
    const float buzzTarget  = levelCurve (snap.nsBuzz) * dbToGain (-58.0f);
    const float printTarget = snap.nsPrintThrough > 0.0f
        ? dbToGain (-45.0f + 13.0f * std::clamp (snap.nsPrintThrough, 0.0f, 1.0f))
        : 0.0f;                                          // -45..-32 dB post-echo

    // Hum harmonic set 1..8, unit RMS before the level scale; nsHumHarm tilts
    // the per-harmonic rolloff from -6 dB/harmonic to -2 dB/harmonic.
    const double humHz = std::clamp ((double) snap.humFreqHz, 20.0, 400.0);
    const bool ntsc = snap.humFreqHz > 55.0f;
    const double buzzHz = ntsc ? 59.94 : 50.0;           // sync buzz fundamental
    const double frameHz = ntsc ? 29.97 : 25.0;          // frame-rate AM

    float humAmp[8];
    {
        const float rolloffDb = -6.0f + 4.0f * std::clamp (snap.nsHumHarm, 0.0f, 1.0f);
        double sum = 0.0;
        for (int k = 0; k < 8; ++k)
        {
            humAmp[k] = dbToGain (rolloffDb * (float) k);
            sum += 0.5 * (double) humAmp[k] * humAmp[k];
        }
        const float norm = float (1.0 / std::sqrt (std::max (sum, 1.0e-12)));
        for (auto& a : humAmp) a *= norm;
    }
    // Odd harmonics 1,3,..,15 at -3 dB per step; unit RMS including the
    // 30 %-depth frame-rate AM power factor.
    float buzzAmp[8];
    {
        double sum = 0.0;
        for (int j = 0; j < 8; ++j)
        {
            buzzAmp[j] = dbToGain (-3.0f * (float) j);
            sum += 0.5 * (double) buzzAmp[j] * buzzAmp[j];
        }
        sum *= 0.85 * 0.85 + 0.5 * 0.15 * 0.15;
        const float norm = float (1.0 / std::sqrt (std::max (sum, 1.0e-12)));
        for (auto& a : buzzAmp) a *= norm;
    }

    // ---- event densities (Poisson, quadratic in amount) -------------------
    const float crk = std::clamp (snap.nsCrackle, 0.0f, 1.0f);
    const float drt = std::clamp (snap.nsDirt, 0.0f, 1.0f);
    const float dro = std::clamp (snap.nsDropout, 0.0f, 1.0f);
    const float mic = std::max (crk, drt);   // microphonics ride the artifact amount
    const double crackleRate = crk > 0.0f ? 0.1 + 79.9 * crk * crk : 0.0;
    const double dirtRate    = drt > 0.0f ? 0.02 + 7.98 * drt * drt : 0.0;
    const double dropRate    = dro > 0.0f ? 0.02 + 1.98 * dro * dro : 0.0;
    const double microRate   = mic > 0.0f ? 0.05 * mic * mic : 0.0;
    const float crackleTopDb = -30.0f + 24.0f * (crk - 1.0f);   // ~-30 dBFS at 1
    const float dirtTopDb    = -20.0f + 24.0f * (drt - 1.0f);   // ~-20 dBFS at 1

    // ---- activity gates: a zero source with a decayed smoother costs nothing
    constexpr float tail = 1.0e-9f;
    const bool hissOn  = hissTarget  > 0.0f || smHiss.state()  > tail;
    const bool cellOn  = cellTarget  > 0.0f || smCell.state()  > tail;
    const bool bcastOn = bcastTarget > 0.0f || smBcast.state() > tail;
    const bool humOn   = humTarget   > 0.0f || smHum.state()   > tail;
    const bool buzzOn  = buzzTarget  > 0.0f || smBuzz.state()  > tail;
    const bool projOn  = projTarget  > 0.0f || smProj.state()  > tail;
    const bool printOn = printTarget > 0.0f || smPrint.state() > tail;
    bool crackleOn = crackleRate > 0.0, dirtOn = dirtRate > 0.0,
         microOn = microRate > 0.0, dropOn = dropRate > 0.0;
    for (const auto& c : channels)
    {
        crackleOn |= c.crackle.remaining > 0;
        dirtOn    |= c.dirt.remaining > 0;
        microOn   |= c.micro.remaining > 0;
        dropOn    |= c.dropout.remaining > 0 || c.dropout.clusterLeft > 0;
    }

    const float widthT = std::clamp (snap.nsWidth, 0.0f, 1.0f);
    // MTS (L-R) noise: the decorrelated portion is 6 dB louder (DSP_SPEC §3).
    const float bcDecor = snap.medium == Medium::broadcastStereo ? 2.0f : 1.0f;
    const float duckAmt = std::clamp (snap.nsDuck, 0.0f, 1.0f);

    const double humInc = humHz / sr, buzzInc = buzzHz / sr;
    const double frameInc = frameHz / sr, cellAmInc = 96.0 / sr;

    for (int i = 0; i < n; ++i)
    {
        // Program envelope on the untouched input (mono sum, pre-noise).
        float monoIn = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            monoIn += buffer.getReadPointer (ch)[i];
        const float env = programEnv.process (monoIn / (float) numCh);

        // Duck continuous noise up to -12 dB when the program is loud
        // (ramps in above -30 dBFS); with nsSilence off, gate it out
        // entirely below about -60 dBFS. Both smoothed ~50 ms.
        float duckT = 1.0f;
        if (duckAmt > 0.0f)
        {
            const float t = std::clamp ((env - 0.0316f) / (0.0631f - 0.0316f), 0.0f, 1.0f);
            duckT = dbToGain (-12.0f * duckAmt * t);
        }
        if (! snap.nsSilence)
        {
            const float g = std::clamp ((0.001f - env) / (0.001f - 0.000316f), 0.0f, 1.0f);
            duckT *= 1.0f - g;
        }
        const float duck = smDuck.process (duckT);
        const float w = smWidth.process (widthT);

        // Shared (correlated) draws — one per sample regardless of channels.
        float hissA = 0, cellA = 0, bcastA = 0;
        float sharedHiss = 0, sharedCell = 0, sharedBcast = 0;
        float normW = 1.0f, normBc = 1.0f, cellAm = 1.0f;
        if (hissOn || cellOn)   // energy-preserving width mix normalization
            normW = 1.0f / std::sqrt ((1.0f - w) * (1.0f - w) + w * w);
        if (hissOn)
        {
            hissA = smHiss.process (hissTarget) * duck;
            sharedHiss = hum.hissShared.nextBipolar();
        }
        if (cellOn)
        {
            cellA = smCell.process (cellTarget) * duck;
            sharedCell = hum.cellShared.nextBipolar();
            cellAm = 0.9f + 0.1f * std::cos (kTwoPi * (float) hum.cellAmPhase);
            hum.cellAmPhase += cellAmInc; if (hum.cellAmPhase >= 1.0) hum.cellAmPhase -= 1.0;
        }
        if (bcastOn)
        {
            bcastA = smBcast.process (bcastTarget) * duck;
            sharedBcast = hum.bcastShared.nextBipolar();
            normBc = 1.0f / std::sqrt ((1.0f - w) * (1.0f - w)
                                       + bcDecor * bcDecor * w * w);
        }

        float humV = 0.0f;
        if (humOn)
        {
            const float a = smHum.process (humTarget) * duck;
            const float ph = (float) hum.phase;
            float s = 0.0f;
            for (int k = 0; k < 8; ++k)
                s += humAmp[k] * std::sin (kTwoPi * (float) (k + 1) * ph);
            humV = a * s;
            hum.phase += humInc; if (hum.phase >= 1.0) hum.phase -= 1.0;
        }
        float buzzV = 0.0f;
        if (buzzOn)
        {
            const float a = smBuzz.process (buzzTarget) * duck;
            const float ph = (float) hum.buzzPhase;
            float s = 0.0f;
            for (int j = 0; j < 8; ++j)
                s += buzzAmp[j] * std::sin (kTwoPi * (float) (2 * j + 1) * ph);
            const float am = 0.85f + 0.15f * std::cos (kTwoPi * (float) hum.buzzAmPhase);
            buzzV = a * s * am;
            hum.buzzPhase += buzzInc;    if (hum.buzzPhase >= 1.0)   hum.buzzPhase -= 1.0;
            hum.buzzAmPhase += frameInc; if (hum.buzzAmPhase >= 1.0) hum.buzzAmPhase -= 1.0;
        }
        float projV = 0.0f;
        if (projOn)     // level-independent gate texture (present in silence)
        {
            const float a = smProj.process (projTarget) * duck;
            projV = a * projectorSample (hum.projPhase, hum.projAmPhase,
                                         hum.projBp, hum.projThumpLp,
                                         hum.projRng, sr);
        }
        const float printG = printOn ? smPrint.process (printTarget) : 0.0f;

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto& c = channels[(size_t) ch];
            float* d = buffer.getWritePointer (ch);
            const float input = d[i];       // program as delivered to us
            float x = input;

            if (dropOn)                     // modifies the program itself
                x = processDropout (c, x, dropRate, dro);

            float y = x;
            if (hissOn)
            {
                const float mixed = normW * ((1.0f - w) * sharedHiss
                                             + w * c.hissRng.nextBipolar());
                float shaped;
                switch (spectrum)
                {
                    case HissSpectrum::dark:
                        shaped = darkHiss (c.hissDarkLp, mixed); break;
                    case HissSpectrum::optical:
                        shaped = c.hissCellShelf.process (mixed); break;
                    default:    // broadcast media: dark floor + FM-tilt floor
                        shaped = 0.70710678f * (darkHiss (c.hissDarkLp, mixed)
                                 + bcastTilt (c.hissBcPrev, bcastDiffK, mixed));
                        break;
                }
                y += hissA * shaped;
            }
            if (cellOn)
            {
                const float mixed = normW * ((1.0f - w) * sharedCell
                                             + w * c.cellRng.nextBipolar());
                y += cellA * cellAm * c.cellShelf.process (mixed);
            }
            if (bcastOn)
            {
                const float mixed = normBc * ((1.0f - w) * sharedBcast
                                              + bcDecor * w * c.bcastRng.nextBipolar());
                y += bcastA * bcastTilt (c.bcastPrev, bcastDiffK, mixed);
            }
            y += humV + buzzV + projV;      // correlated media-borne tones

            // Impulsive artifacts never duck.
            if (crackleOn) y += crackleSample (c, crackleRate, crackleTopDb);
            if (dirtOn)    y += dirtSample (c, dirtRate, dirtTopDb);
            if (microOn)   y += microSample (c, microRate);

            if (printOn)                    // DEV-002: post-echo only (MVP)
            {
                auto& p = printThrough[(size_t) ch];
                const float echo = p.lp.process (p.delay[(size_t) p.writePos]);
                p.delay[(size_t) p.writePos] = input;
                if (++p.writePos > printDelaySamples) p.writePos = 0;
                y += printG * echo;
            }

            d[i] = y;
        }
    }
}

} // namespace vfa::dsp
