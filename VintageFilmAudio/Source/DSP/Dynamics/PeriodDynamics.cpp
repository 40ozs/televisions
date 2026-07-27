// Period dynamics (DSP_SPEC §7): AGC -> dialogue focus -> program compressor
// -> peak limiter, all on a channel-linked (max-of-channels) detector.
//
// Calibration summary (nominal level -18 dBFS = 0 VU, DSP_SPEC preamble):
//  * AGC       target -18 dBFS, correction bounded +-12 dB * agc, attack
//              300 ms, release 4 s * (1 - relChar*0.5) shortened toward
//              400 ms by `pump` (pump also deepens correction x1.3 —
//              HIST-APPROX audible pumping). Gated below -45 dBFS.
//  * Dialogue  +4 dB * dialog presence peak @ 2 kHz Q 0.8 plus a gentle
//              12 dB/oct low cut at 80+120*dialog Hz (Q 0.9, slightly
//              underdamped so 150 Hz survives — intelligibility tilt,
//              NOT a telephone band-pass).
//  * Compressor threshold -18 dBFS, ratio 1.5 + 2.5*comp, soft knee
//              9 dB -> 2 dB as relChar goes vari-mu -> FET, attack
//              15 ms -> 1 ms, dual release 150 ms/1.2 s (program-dependent,
//              blended by crest factor) -> 80 ms single.
//              Makeup: makeupDb = 0.5 * comp * 6 dB * (1 - 1/ratio) —
//              approximately restores a program riding ~6 dB above the
//              -18 dBFS nominal back toward unity.
//  * Limiter   threshold -8 .. -14 dBFS as `limit` rises, attack
//              5 ms -> 1 ms per relChar, ~20:1 (slope 0.95), 1.5 dB knee,
//              dual-time release (60 ms fast + 500 ms peak-hold tail).
//
// currentGainReductionDb() reports POSITIVE dB of reduction (comp+limiter).
// Every stage is fully bypassed (zero CPU) when its amount is 0.

#include "PeriodDynamics.h"

namespace vfa::dsp
{

namespace
{
constexpr float kNominalDb  = -18.0f;        // 0 VU reference level
constexpr float kAgcMaxDb   = 12.0f;         // max correction at agc == 1
constexpr float kAgcGateLin = 0.00562341f;   // -45 dBFS, linear

float onePoleCoeff (double sr, float ms) noexcept
{
    return 1.0f - std::exp (-1.0f / (std::max (ms, 0.01f) * 0.001f * float (sr)));
}

// Positive dB of reduction for a level `overDb` above threshold, quadratic
// soft knee of width kneeDb, slope = 1 - 1/ratio.
float softKneeGrDb (float overDb, float kneeDb, float slope) noexcept
{
    if (2.0f * overDb <= -kneeDb)
        return 0.0f;
    if (2.0f * overDb >= kneeDb)
        return slope * overDb;
    const float t = overDb + kneeDb * 0.5f;
    return slope * t * t / (2.0f * kneeDb);
}

float linkedAbs (float* const* ch, int numCh, int i) noexcept
{
    float link = std::abs (ch[0][i]);
    for (int c = 1; c < numCh; ++c)
        link = std::max (link, std::abs (ch[c][i]));
    return link;
}
} // namespace

void PeriodDynamics::prepare (const StreamSpec& spec)
{
    streamSpec = spec;
    agcDetector.prepare (spec.sampleRate, 50.0f, 300.0f);
    agcGate.prepare (spec.sampleRate, 2.0f, 50.0f);
    compDetector.prepare (spec.sampleRate, 15.0f, 150.0f);
    limitDetector.prepare (spec.sampleRate, 3.0f, 60.0f);
    crestPeak.prepare (spec.sampleRate, 1.0f, 500.0f);

    agcAttackCoeff    = onePoleCoeff (spec.sampleRate, 300.0f);
    agcSmoothCoeff    = onePoleCoeff (spec.sampleRate, 100.0f);
    gainSmoothCoeff   = onePoleCoeff (spec.sampleRate, 2.0f);
    compSlowRelCoeff  = onePoleCoeff (spec.sampleRate, 1200.0f);
    limitSlowRelCoeff = onePoleCoeff (spec.sampleRate, 500.0f);
    crestRmsCoeff     = onePoleCoeff (spec.sampleRate, 400.0f);

    eq.assign ((size_t) spec.numChannels, {});
    reset();
}

void PeriodDynamics::reset()
{
    agcDetector.reset();
    agcGate.reset();
    compDetector.reset();
    limitDetector.reset();
    crestPeak.reset();
    for (auto& e : eq) { e.presence.reset(); e.lowCut.reset(); }
    agcRiderDb = 0.0f;
    agcGain = compGain = limitGain = 1.0f;
    compGrSlowDb = limitGrSlowDb = 0.0f;
    crestRmsSq = 0.0f;
    slowWeight = 1.0f;
    lastDialog = -1.0f;
    grDb = 0.0f;
}

void PeriodDynamics::process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap)
{
    const int n = buffer.getNumSamples();
    const int numCh = std::min (buffer.getNumChannels(), (int) eq.size());
    if (n <= 0 || numCh <= 0)
        return;

    auto* const* ch = buffer.getArrayOfWritePointers();
    const float relChar = std::clamp (snap.relChar, 0.0f, 1.0f);
    const float pump    = std::clamp (snap.pump, 0.0f, 1.0f);

    // ------------------------------------------------------------- 1. AGC
    const float agcAmt = std::clamp (snap.agc, 0.0f, 1.0f);
    if (agcAmt > 1.0e-4f)
    {
        // Release 4 s at vari-mu -> 2 s at FET; Pumping pulls it toward
        // 400 ms and deepens the correction (HIST-APPROX).
        const float baseRelSec = 4.0f * (1.0f - 0.5f * relChar);
        const float relSec = baseRelSec + (0.4f - baseRelSec) * pump;
        const float agcReleaseCoeff = onePoleCoeff (streamSpec.sampleRate, relSec * 1000.0f);
        const float depth = 1.0f + 0.3f * pump;
        const float maxDb = kAgcMaxDb * agcAmt;   // amount scales max correction

        for (int i = 0; i < n; ++i)
        {
            const float link = linkedAbs (ch, numCh, i);
            if (agcGate.process (link) > kAgcGateLin)   // gate: freeze < -45 dBFS
            {
                const float envDb  = gainToDb (agcDetector.process (link));
                const float target = std::clamp (depth * (kNominalDb - envDb),
                                                 -maxDb, maxDb);
                agcRiderDb += (target < agcRiderDb ? agcAttackCoeff : agcReleaseCoeff)
                            * (target - agcRiderDb);
            }
            agcGain += agcSmoothCoeff * (dbToGain (agcRiderDb) - agcGain);
            for (int c = 0; c < numCh; ++c)
                ch[c][i] *= agcGain;
        }
    }
    else
    {
        agcRiderDb = 0.0f;   // park neutral so re-engage starts clean
        agcGain = 1.0f;
    }

    // -------------------------------------------------- 2. dialogue focus
    const float dialog = std::clamp (snap.dialog, 0.0f, 1.0f);
    if (dialog > 1.0e-4f)
    {
        if (std::abs (dialog - lastDialog) > 0.005f)   // material change only
        {
            lastDialog = dialog;
            for (auto& e : eq)
            {
                e.presence.peak (streamSpec.sampleRate, 2000.0f, 0.8f, 4.0f * dialog);
                e.lowCut.highpass (streamSpec.sampleRate, 80.0f + 120.0f * dialog, 0.9f);
            }
        }
        for (int c = 0; c < numCh; ++c)
        {
            auto& e = eq[(size_t) c];
            float* d = ch[c];
            for (int i = 0; i < n; ++i)
                d[i] = e.presence.process (e.lowCut.process (d[i]));
        }
    }

    // --------------------------------------------- 3. program compressor
    const float comp = std::clamp (snap.comp, 0.0f, 1.0f);
    float compGrNow = 0.0f;
    if (comp > 1.0e-4f)
    {
        const float ratio     = 1.5f + 2.5f * comp;
        const float slope     = 1.0f - 1.0f / ratio;
        const float kneeDb    = 9.0f - 7.0f * relChar;       // vari-mu -> FET
        const float attackMs  = 15.0f - 14.0f * relChar;
        const float fastRelMs = 150.0f - 70.0f * relChar;
        compDetector.setTimes (attackMs, fastRelMs);
        const float makeup = dbToGain (0.5f * comp * 6.0f * slope);

        // Program-dependent slow-release weight from crest factor: dense
        // program (crest ~3 dB) gets the full 1.2 s vari-mu tail, spiky
        // program mostly the fast release (floor 0.35 keeps the character);
        // FET (relChar = 1) removes the slow tail entirely.
        const float peakV = crestPeak.value();
        const float rmsV  = std::sqrt (std::max (crestRmsSq, 0.0f));
        const float crestDb = (peakV > 1.0e-6f && rmsV > 1.0e-6f)
                            ? gainToDb (peakV) - gainToDb (rmsV) : 3.0f;
        const float crestW  = std::clamp ((12.0f - crestDb) / 9.0f, 0.0f, 1.0f);
        const float targetW = (1.0f - relChar) * (0.35f + 0.65f * crestW);
        slowWeight += 0.1f * (targetW - slowWeight);

        for (int i = 0; i < n; ++i)
        {
            const float link = linkedAbs (ch, numCh, i);
            crestPeak.process (link);
            crestRmsSq = flushToZero (crestRmsSq + crestRmsCoeff * (link * link - crestRmsSq));

            const float envDb = gainToDb (compDetector.process (link));
            const float raw   = softKneeGrDb (envDb - kNominalDb, kneeDb, slope);

            compGrSlowDb = flushToZero (compGrSlowDb - compSlowRelCoeff * compGrSlowDb);
            compGrSlowDb = std::max (compGrSlowDb, raw);     // peak-hold tail

            const float gr = std::max (raw, slowWeight * compGrSlowDb);
            compGain += gainSmoothCoeff * (makeup * dbToGain (-gr) - compGain);
            for (int c = 0; c < numCh; ++c)
                ch[c][i] *= compGain;
            compGrNow = gr;
        }
    }
    else
    {
        compGain = 1.0f;
        compGrSlowDb = 0.0f;
    }

    // -------------------------------------------------- 4. peak limiter
    const float limit = std::clamp (snap.limit, 0.0f, 1.0f);
    float limitGrNow = 0.0f;
    if (limit > 1.0e-4f)
    {
        const float thresholdDb = -8.0f - 6.0f * limit;      // -8 .. -14 dBFS
        limitDetector.setTimes (5.0f - 4.0f * relChar, 60.0f);
        constexpr float kneeDb = 1.5f, slope = 0.95f;        // hard-ish, ~20:1
        const float slowW = 0.4f + 0.3f * (1.0f - relChar);

        for (int i = 0; i < n; ++i)
        {
            const float link  = linkedAbs (ch, numCh, i);
            const float envDb = gainToDb (limitDetector.process (link));
            const float raw   = softKneeGrDb (envDb - thresholdDb, kneeDb, slope);

            limitGrSlowDb = flushToZero (limitGrSlowDb - limitSlowRelCoeff * limitGrSlowDb);
            limitGrSlowDb = std::max (limitGrSlowDb, raw);

            const float gr = std::max (raw, slowW * limitGrSlowDb);
            limitGain += gainSmoothCoeff * (dbToGain (-gr) - limitGain);
            for (int c = 0; c < numCh; ++c)
                ch[c][i] *= limitGain;
            limitGrNow = gr;
        }
    }
    else
    {
        limitGain = 1.0f;
        limitGrSlowDb = 0.0f;
    }

    grDb = compGrNow + limitGrNow;   // metering: positive dB of reduction
}

} // namespace vfa::dsp
