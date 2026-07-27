// Magnetic film / tape model (DSP_SPEC §3). MVP saturation path: an
// odd-symmetric fastTanh shaper (no DC, no even harmonics) inside an
// OversampledStage, with drive- and level-dependent HF loss before it
// (bias / self-erasure) and a partial inverse shelf after it. The head bump
// tracks tape speed; band edges, bump and saturation severity are selected
// per medium variant from snap.medium. HIST-APPROX values throughout, per
// the §3 table. Post-MVP replacement path: Jiles-Atherton behind this API.
#include "MagneticModel.h"

namespace vfa::dsp
{

namespace
{
// Per-variant configuration (DSP_SPEC §3 table, HIST-APPROX). gapLossHz is
// placed so that the *total* chain (gap loss + resting pre-shaper loss +
// partial restore) measures ~-3 dB at the table's upper bandwidth limit.
struct VariantConfig
{
    float lowHz;           // lower -3 dB corner (2nd-order highpass)
    float gapLossHz;       // top-end 2nd-order lowpass ("gap loss")
    float driveOffsetDb;   // saturation severity re magnetic film
    float azimuthDb;       // consumer: fixed azimuth-style HF shelf plateau
};

constexpr VariantConfig kVariants[] =
{
    { 40.0f, 15000.0f, 0.0f,  0.0f },   // magneticFilm: 40 Hz-14 kHz, mildest
    { 50.0f, 12800.0f, 1.5f,  0.0f },   // fieldTape:    50 Hz-12 kHz
    { 60.0f, 14000.0f, 4.0f, -5.0f },   // consumer:     60 Hz-9 kHz, heaviest
};

int variantIndexFor (Medium m) noexcept
{
    if (m == Medium::fieldTape) return 1;
    if (m == Medium::consumer)  return 2;
    return 0;                   // magneticFilm, and any non-magnetic fallback
}

// Saturation drive mapping. Calibrated to the DSP_SPEC reference level:
// -18 dBFS 1 kHz at magSat 0.4 gives ~1.5 % THD (fastTanh small-signal THD
// ~= (g*A)^2 / 12) with <=0.5 dB gain error at 0 VU.
constexpr float kDriveBaseDb  = 4.9f;      // magSat 0 drive
constexpr float kDriveRangeDb = 14.0f;     // magSat 0..1 span
constexpr float kNominalAmp   = 0.1258925f; // -18 dBFS = 0 VU

// Level-dependent HF loss (bias / self-erasure, HIST-APPROX): a one-pole
// cutoff that slides from 16 kHz down to ~5 kHz at full drive + full level.
constexpr float kPreLpBaseHz    = 16000.0f;
constexpr float kPreLpLnRatio   = -1.1631508f; // ln (5 kHz / 16 kHz)
constexpr float kEnvNominal     = 0.35f;       // envelope treated as "full level"
constexpr float kLossDriveW     = 0.5f;        // drive weight in the loss amount
constexpr float kLossEnvW       = 0.7f;        // envelope weight in the loss amount
constexpr float kRestoreRefHz   = 10000.0f;    // restore ~50 % measured here
constexpr float kRestoreShelfHz = 5000.0f;

// Head bump (HIST-APPROX): up to +2.5 dB peaking, frequency from tape speed.
constexpr float kBumpMaxDb = 2.5f;
constexpr float kBumpQ     = 1.2f;

constexpr int   kCtrlInterval  = 16;       // control-rate cutoff refresh
constexpr float kSmoothSeconds = 0.030f;   // block-rate control smoothing
} // namespace

float MagneticModel::headBumpHzFor (TapeSpeed s) noexcept
{
    switch (s)
    {
        case TapeSpeed::ips3_75: return 90.0f;
        case TapeSpeed::ips7_5:  return 70.0f;
        case TapeSpeed::ips15:   return 55.0f;
        case TapeSpeed::ips30:   return 40.0f;
    }
    return 55.0f;
}

void MagneticModel::prepare (const StreamSpec& spec)
{
    streamSpec = spec;
    channels.assign ((size_t) spec.numChannels, {});
    for (auto& c : channels)
    {
        c.levelEnv.prepare (spec.sampleRate, 5.0f, 50.0f);
        c.dc.prepare (spec.sampleRate);
    }
    os.prepare (spec);
    smoothedDriveDb = -1000.0f;             // snap to target on first block
    smoothedBumpDb  = 0.0f;
    cachedVariant = cachedSpeed = -1;       // force coefficient refresh
    cachedBumpDb = cachedRestoreDb = -1000.0f;
}

void MagneticModel::reset()
{
    for (auto& c : channels)
    {
        c.headBump.reset(); c.hfLossPre.reset(); c.hfRestore.reset();
        c.gapLoss.reset(); c.azimuthLoss.reset(); c.lowCut.reset();
        c.levelEnv.reset(); c.dc.reset();
    }
    os.reset();
}

void MagneticModel::process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap,
                             float extraDriveDb)
{
    const int numSamples = buffer.getNumSamples();
    const int numCh = std::min (buffer.getNumChannels(), (int) channels.size());
    if (numSamples == 0 || numCh == 0)
        return;

    const double sr = streamSpec.sampleRate;
    const int variant = variantIndexFor (snap.medium);
    const auto& cfg = kVariants[variant];

    // Block-rate control smoothing: no zipper on drive / bump moves.
    const float targetDriveDb = kDriveBaseDb
                              + kDriveRangeDb * std::clamp (snap.magSat, 0.0f, 1.0f)
                              + cfg.driveOffsetDb + extraDriveDb;
    const float targetBumpDb = std::clamp (snap.headBump, 0.0f, 1.0f) * kBumpMaxDb;
    if (smoothedDriveDb <= -999.0f)
    {
        smoothedDriveDb = targetDriveDb;
        smoothedBumpDb  = targetBumpDb;
    }
    const float smooth = 1.0f - std::exp (-float (numSamples) / (kSmoothSeconds * float (sr)));
    smoothedDriveDb += smooth * (targetDriveDb - smoothedDriveDb);
    smoothedBumpDb  += smooth * (targetBumpDb  - smoothedBumpDb);

    const float driveLin = dbToGain (smoothedDriveDb);
    const float makeup   = kNominalAmp / fastTanh (driveLin * kNominalAmp); // 0 VU stays put
    const float drive01  = std::clamp ((smoothedDriveDb - kDriveBaseDb) / kDriveRangeDb,
                                       0.0f, 1.25f);

    // Partial (~50 %) inverse of the drive-only part of the pre-shaper loss,
    // referenced at 10 kHz (the envelope-dependent part is *not* restored).
    const float restingCut = kPreLpBaseHz * std::exp (kLossDriveW * drive01 * kPreLpLnRatio);
    const float rr = kRestoreRefHz / restingCut;
    const float restoreDb = 0.5f * 10.0f * std::log10 (1.0f + rr * rr);

    // Static filters: coefficients recomputed only on a real change; filter
    // state always carries across so there is no click on switches.
    const int speedIdx = (int) snap.tapeSpeed;
    if (variant != cachedVariant || speedIdx != cachedSpeed
        || std::abs (smoothedBumpDb - cachedBumpDb) > 0.02f
        || std::abs (restoreDb - cachedRestoreDb) > 0.02f)
    {
        cachedVariant   = variant;
        cachedSpeed     = speedIdx;
        cachedBumpDb    = smoothedBumpDb;
        cachedRestoreDb = restoreDb;
        const float bumpHz = headBumpHzFor (snap.tapeSpeed);
        for (auto& c : channels)
        {
            c.headBump.peak (sr, bumpHz, kBumpQ, smoothedBumpDb);
            c.hfRestore.highShelf (sr, kRestoreShelfHz, 1.0f, restoreDb);
            c.gapLoss.lowpass (sr, cfg.gapLossHz, 0.7071f);
            c.lowCut.highpass (sr, cfg.lowHz, 0.7071f);
            if (cfg.azimuthDb != 0.0f)
                c.azimuthLoss.highShelf (sr, 8000.0f, 1.0f, cfg.azimuthDb);
            else
                c.azimuthLoss.setIdentity();
        }
    }

    // Head bump, then the level/drive-dependent pre-shaper HF loss. The
    // envelope runs per sample; the cutoff is refreshed at control rate
    // (every 16 samples) from the already-smooth envelope -> no zipper.
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& c = channels[(size_t) ch];
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = c.headBump.process (d[i]);
            const float env = c.levelEnv.process (x);
            if ((i % kCtrlInterval) == 0)
            {
                const float envN = std::min (env * (1.0f / kEnvNominal), 1.0f);
                const float lossAmt = std::clamp (drive01 * (kLossDriveW + kLossEnvW * envN),
                                                  0.0f, 1.0f);
                c.hfLossPre.setCutoff (kPreLpBaseHz * std::exp (lossAmt * kPreLpLnRatio), sr);
            }
            d[i] = c.hfLossPre.process (x);
        }
    }

    // Odd-symmetric saturator, oversampled (eco 1x / standard 2x / high 4x).
    os.process (buffer, snap.quality,
                [driveLin, makeup] (float* const* chans, int nCh, int n, double)
                {
                    for (int c = 0; c < nCh; ++c)
                    {
                        float* s = chans[c];
                        for (int i = 0; i < n; ++i)
                            s[i] = fastTanh (driveLin * s[i]) * makeup;
                    }
                });

    // Partial HF restore, azimuth/gap loss band edges, DC safety.
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto& c = channels[(size_t) ch];
        auto* d = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            float x = c.hfRestore.process (d[i]);
            x = c.azimuthLoss.process (x);
            x = c.gapLoss.process (x);
            x = c.lowCut.process (x);
            d[i] = c.dc.process (x);
        }
    }
}

} // namespace vfa::dsp
