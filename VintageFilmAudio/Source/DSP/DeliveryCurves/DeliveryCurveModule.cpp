#include "DeliveryCurveModule.h"
#include "MinPhaseFirDesigner.h"

namespace vfa::dsp
{

namespace
{
float curveAmountFor (const ParamSnapshot& snap) noexcept
{
    if (snap.curveMode == CurveMode::academy)
        return snap.academyAmt;
    if (snap.curveMode == CurveMode::xCurve || snap.curveMode == CurveMode::xCurveSmallRoom)
        return snap.xcurveAmt;
    return 1.0f;
}

// dB shaping applied in the FIR region (>= crossover): mid shape, HF roll,
// large-room air loss. LF effects are analytic runtime filters instead.
float advancedHfShapeDb (const ParamSnapshot& snap, float hz)
{
    float dB = 0.0f;
    if (hz > snap.delHfRollHz)
        dB -= 12.0f * std::log2 (hz / snap.delHfRollHz);
    if (snap.delMidShapeDb != 0.0f)
    {
        const float d = std::abs (std::log2 (hz / 1500.0f)) / 1.25f;
        if (d < 1.0f)
            dB += snap.delMidShapeDb * 0.5f * (1.0f + std::cos (kPi * d));
    }
    // Playback size > 0.5: progressively more air absorption (HIST-APPROX).
    const float air = std::max (0.0f, (snap.playbackSize - 0.5f) * 2.0f);
    if (air > 0.0f && hz > 4000.0f)
        dB -= air * 2.0f * std::log2 (hz / 4000.0f);
    return dB;
}

float smallRoomHpHz (const ParamSnapshot& snap) noexcept
{
    // Playback size < 0.5: small systems lose LF extension (HIST-APPROX).
    const float small = std::max (0.0f, (0.5f - snap.playbackSize) * 2.0f);
    return small <= 0.001f ? 0.0f : 15.0f + 55.0f * small;
}
} // namespace

int DeliveryCurveModule::tapCountFor (Quality q, double sampleRate) noexcept
{
    const float srScale = sampleRate > 50000.0 ? (sampleRate > 100000.0 ? 2.0f : 1.5f) : 1.0f;
    int base = 257;
    switch (q)
    {
        case Quality::eco:      base = 129; break;
        case Quality::standard: base = 257; break;
        case Quality::high:     base = 513; break;
    }
    return std::min (maxTaps, (int) (float (base) * srScale) | 1);
}

double DeliveryCurveModule::biquadMagnitudeDb (const Biquad& b, double hz, double sampleRate)
{
    const double w = 2.0 * M_PI * hz / sampleRate;
    const std::complex<double> z1 (std::cos (-w), std::sin (-w));
    const std::complex<double> z2 = z1 * z1;
    const auto num = (double) b.b0 + (double) b.b1 * z1 + (double) b.b2 * z2;
    const auto den = 1.0 + (double) b.a1 * z1 + (double) b.a2 * z2;
    return 20.0 * std::log10 (std::max (std::abs (num / den), 1.0e-9));
}

DeliveryCurveModule::LfFit DeliveryCurveModule::fitLowShelves (
    const std::vector<curves::BreakPoint>& points, float amount, double sampleRate)
{
    // Relative LF target (dB below the crossover, re the crossover value).
    static constexpr float fitFreqs[] = { 25, 31.5f, 40, 50, 63, 80, 100, 125, 160, 200 };
    const float refDb = MinPhaseFirDesigner::interpolateDb (points, lfCrossoverHz) * amount;

    float target[10];
    float maxAbs = 0.0f;
    for (int i = 0; i < 10; ++i)
    {
        target[i] = MinPhaseFirDesigner::interpolateDb (points, fitFreqs[i]) * amount - refDb;
        maxAbs = std::max (maxAbs, std::abs (target[i]));
    }
    LfFit fit;
    if (maxAbs < 0.3f)
        return fit;   // flat LF: identity shelves

    static constexpr float fcs[] = { 20, 25, 32, 40, 50, 63, 80, 100, 126, 159, 200 };
    static constexpr int numFc = 11;
    static constexpr int numG = 22;   // 0 .. -15.75 dB in 0.75 steps

    // Precompute each candidate section's response at the fit frequencies.
    static thread_local std::vector<float> table;   // [fc][g][freq]
    table.assign ((size_t) numFc * numG * 10, 0.0f);
    for (int fi = 0; fi < numFc; ++fi)
        for (int gi = 0; gi < numG; ++gi)
        {
            Biquad b;
            b.lowShelf (sampleRate, fcs[fi], 1.0f, -0.75f * (float) gi);
            for (int k = 0; k < 10; ++k)
                table[(size_t) ((fi * numG + gi) * 10 + k)] =
                    (float) (biquadMagnitudeDb (b, fitFreqs[k], sampleRate)
                             - biquadMagnitudeDb (b, lfCrossoverHz, sampleRate));
        }

    float bestErr = 1.0e9f;
    for (int f1 = 0; f1 < numFc; ++f1)
        for (int g1 = 0; g1 < numG; ++g1)
            for (int f2 = f1; f2 < numFc; ++f2)
                for (int g2 = 0; g2 < numG; ++g2)
                {
                    const float* r1 = &table[(size_t) ((f1 * numG + g1) * 10)];
                    const float* r2 = &table[(size_t) ((f2 * numG + g2) * 10)];
                    float err = 0.0f;
                    for (int k = 0; k < 10; ++k)
                    {
                        const float e = r1[k] + r2[k] - target[k];
                        err = std::max (err, std::abs (e));
                    }
                    if (err < bestErr)
                    {
                        bestErr = err;
                        fit = { fcs[f1], -0.75f * (float) g1, fcs[f2], -0.75f * (float) g2 };
                    }
                }
    return fit;
}

void DeliveryCurveModule::prepare (const StreamSpec& spec)
{
    streamSpec = spec;
    for (auto& s : path)
    {
        s.taps.assign ((size_t) maxTaps, 0.0f);
        s.taps[0] = 1.0f;
        s.len = tapCountFor (Quality::standard, spec.sampleRate);
        s.history.assign ((size_t) spec.numChannels, std::vector<float> ((size_t) maxTaps * 2, 0.0f));
        s.writePos.assign ((size_t) spec.numChannels, 0);
        s.shelf1.assign ((size_t) spec.numChannels, {});
        s.shelf2.assign ((size_t) spec.numChannels, {});
        s.fit = {};
    }
    lfRollHp.assign ((size_t) spec.numChannels, {});
    sizeHp.assign ((size_t) spec.numChannels, {});
    lastLfRollHz = lastSizeHpHz = -1.0f;
    currentIndex = 0;
    pending.store (0, std::memory_order_release);
    fading = false;
    fade = 0.0f;
    fadeStep = 1.0f / std::max (1.0f, float (spec.sampleRate) * 0.010f);
}

void DeliveryCurveModule::reset()
{
    for (auto& s : path)
    {
        for (auto& h : s.history) std::fill (h.begin(), h.end(), 0.0f);
        std::fill (s.writePos.begin(), s.writePos.end(), 0);
        for (auto& b : s.shelf1) b.reset();
        for (auto& b : s.shelf2) b.reset();
    }
    for (auto& b : lfRollHp) b.reset();
    for (auto& b : sizeHp) b.reset();
}

std::vector<float> DeliveryCurveModule::effectiveMagnitudeGrid (const ParamSnapshot& snap,
                                                                int gridSize, double sampleRate)
{
    auto points = curves::breakpointsFor (snap.curveMode);
    const float amount = curveAmountFor (snap);
    const float sizeHp_ = smallRoomHpHz (snap);

    std::vector<float> grid ((size_t) gridSize);
    const double nyquist = sampleRate * 0.5;
    for (int i = 0; i < gridSize; ++i)
    {
        const float hz = float (std::max (nyquist * double (i) / double (gridSize - 1), 1.0));
        float dB = MinPhaseFirDesigner::interpolateDb (points, hz) * amount
                 + advancedHfShapeDb (snap, hz);
        if (hz < snap.delLfRollHz)
            dB -= 12.0f * std::log2 (snap.delLfRollHz / hz);
        if (sizeHp_ > 0.0f && hz < sizeHp_)
            dB -= 12.0f * std::log2 (sizeHp_ / hz);
        grid[(size_t) i] = dbToGain (std::max (dB, -80.0f));
    }
    return grid;
}

void DeliveryCurveModule::designInto (PathState& state, const ParamSnapshot& snap)
{
    constexpr int fftSize = 8192;
    const int numTaps = tapCountFor (snap.quality, streamSpec.sampleRate);
    const auto& points = curves::breakpointsFor (snap.curveMode);
    const float amount = curveAmountFor (snap);

    // LF: fitted shelves.
    state.fit = fitLowShelves (points, amount, streamSpec.sampleRate);
    for (size_t ch = 0; ch < state.shelf1.size(); ++ch)
    {
        state.shelf1[ch].lowShelf (streamSpec.sampleRate, state.fit.fc1, 1.0f, state.fit.g1);
        state.shelf2[ch].lowShelf (streamSpec.sampleRate, state.fit.fc2, 1.0f, state.fit.g2);
    }
    // The shelves are not exactly 0 dB at/above the crossover; measure their
    // residual and fold its inverse into the FIR grid so the product is flat.
    Biquad p1, p2;
    p1.lowShelf (streamSpec.sampleRate, state.fit.fc1, 1.0f, state.fit.g1);
    p2.lowShelf (streamSpec.sampleRate, state.fit.fc2, 1.0f, state.fit.g2);

    // HF: FIR grid, flattened below the crossover.
    const int gridSize = fftSize / 2 + 1;
    std::vector<float> grid ((size_t) gridSize);
    const double nyquist = streamSpec.sampleRate * 0.5;
    const float refDb = MinPhaseFirDesigner::interpolateDb (points, lfCrossoverHz) * amount;
    for (int i = 0; i < gridSize; ++i)
    {
        const float hz = float (std::max (nyquist * double (i) / double (gridSize - 1), 1.0));
        float dB;
        if (hz >= lfCrossoverHz)
            dB = MinPhaseFirDesigner::interpolateDb (points, hz) * amount
               + advancedHfShapeDb (snap, hz);
        else
            dB = refDb + advancedHfShapeDb (snap, std::max (hz, lfCrossoverHz));
        // Remove the shelves' residual above ~100 Hz so shelf * FIR == target.
        if (hz > 100.0f)
            dB -= float (biquadMagnitudeDb (p1, hz, streamSpec.sampleRate)
                       + biquadMagnitudeDb (p2, hz, streamSpec.sampleRate));
        // Normalize so the crossover region sits at refDb exactly.
        grid[(size_t) i] = dbToGain (std::max (dB - refDb, -80.0f));
    }
    // Restore the absolute level (refDb) as broadband gain in the taps.
    auto taps = MinPhaseFirDesigner::design (grid, numTaps, fftSize);
    const float makeup = dbToGain (refDb);
    for (auto& t : taps) t *= makeup;

    std::copy (taps.begin(), taps.end(), state.taps.begin());
    std::fill (state.taps.begin() + numTaps, state.taps.end(), 0.0f);
    state.len = numTaps;
}

void DeliveryCurveModule::designNow (const ParamSnapshot& snap)
{
    designInto (path[0], snap);
    designInto (path[1], snap);
    currentIndex.store (0, std::memory_order_relaxed);
    pending.store (0, std::memory_order_release);
    fading = false;
}

void DeliveryCurveModule::adoptPendingImmediately() noexcept
{
    if (pending.load (std::memory_order_acquire) != 1 || fading)
        return;
    // Output is bypassed: swap without fading and clear both paths' state so
    // re-enable starts clean. Bounded fills, no allocation.
    currentIndex.store (1 - currentIndex.load (std::memory_order_relaxed),
                        std::memory_order_relaxed);
    for (auto& s : path)
    {
        for (auto& h : s.history) std::fill (h.begin(), h.end(), 0.0f);
        std::fill (s.writePos.begin(), s.writePos.end(), 0);
        for (auto& b : s.shelf1) b.reset();
        for (auto& b : s.shelf2) b.reset();
    }
    pending.store (0, std::memory_order_release);
}

bool DeliveryCurveModule::requestRedesign (const ParamSnapshot& snap)
{
    // Order matters (F4): check the pending handoff flag FIRST — while it is
    // 0 the audio thread will not swap currentIndex, so reading next() after
    // the acquire is race-free. Then guard against pre-prepare calls.
    if (pending.load (std::memory_order_acquire) != 0)
        return false;
    if (next().taps.size() < (size_t) maxTaps)
        return false;
    designInto (next(), snap);
    pending.store (1, std::memory_order_release);
    return true;
}

float DeliveryCurveModule::processSamplePath (PathState& s, int ch, float x) noexcept
{
    // LF shelves first (cheap), then FIR with mirrored contiguous history.
    x = s.shelf1[(size_t) ch].process (x);
    x = s.shelf2[(size_t) ch].process (x);

    auto& h = s.history[(size_t) ch];
    int& wp = s.writePos[(size_t) ch];
    if (--wp < 0) wp += maxTaps;
    h[(size_t) wp] = x;
    h[(size_t) (wp + maxTaps)] = x;

    const float* hist = h.data() + wp;
    const float* taps = s.taps.data();
    float acc = 0.0f;
    for (int i = 0; i < s.len; ++i)
        acc += taps[i] * hist[i];
    return acc;
}

void DeliveryCurveModule::process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap)
{
    const int numCh = std::min (buffer.getNumChannels(), (int) current().history.size());
    const int n = buffer.getNumSamples();

    // Analytic runtime LF filters (coefficients refreshed on real change).
    const float lfHz = snap.delLfRollHz;
    const bool lfActive = lfHz > 21.0f;
    if (lfActive && std::abs (lfHz - lastLfRollHz) > 0.01f * std::max (lfHz, 1.0f))
    {
        lastLfRollHz = lfHz;
        for (auto& b : lfRollHp) b.highpass (streamSpec.sampleRate, lfHz, 0.707f);
    }
    const float shpHz = smallRoomHpHz (snap);
    const bool sizeActive = shpHz > 0.0f;
    if (sizeActive && std::abs (shpHz - lastSizeHpHz) > 0.01f * std::max (shpHz, 1.0f))
    {
        lastSizeHpHz = shpHz;
        for (auto& b : sizeHp) b.highpass (streamSpec.sampleRate, shpHz, 0.6f);
    }
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* d = buffer.getWritePointer (ch);
        if (lfActive)
            for (int i = 0; i < n; ++i) d[i] = lfRollHp[(size_t) ch].process (d[i]);
        if (sizeActive)
            for (int i = 0; i < n; ++i) d[i] = sizeHp[(size_t) ch].process (d[i]);
    }

    // Begin crossfade to a staged design.
    if (! fading && pending.load (std::memory_order_acquire) == 1)
    {
        fading = true;
        fade = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            // Element-wise copies (never vector assignment: allocation-free
            // by contract on the audio thread, review finding F4).
            auto& dstH = next().history[(size_t) ch];
            const auto& srcH = current().history[(size_t) ch];
            std::copy (srcH.begin(), srcH.end(), dstH.begin());
            next().writePos[(size_t) ch] = current().writePos[(size_t) ch];
            // Carry filter state across so the fade compares like with like.
            next().shelf1[(size_t) ch].z1 = current().shelf1[(size_t) ch].z1;
            next().shelf1[(size_t) ch].z2 = current().shelf1[(size_t) ch].z2;
            next().shelf2[(size_t) ch].z1 = current().shelf2[(size_t) ch].z1;
            next().shelf2[(size_t) ch].z2 = current().shelf2[(size_t) ch].z2;
        }
    }

    if (! fading)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
                d[i] = processSamplePath (current(), ch, d[i]);
        }
        return;
    }

    for (int i = 0; i < n; ++i)
    {
        fade = std::min (1.0f, fade + fadeStep);
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            const float x = d[i];
            const float a = processSamplePath (current(), ch, x);
            const float b = processSamplePath (next(), ch, x);
            d[i] = a + fade * (b - a);
        }
    }

    if (fade >= 1.0f)
    {
        currentIndex.store (1 - currentIndex.load (std::memory_order_relaxed),
                            std::memory_order_relaxed);
        fading = false;
        pending.store (0, std::memory_order_release);
    }
}

} // namespace vfa::dsp
