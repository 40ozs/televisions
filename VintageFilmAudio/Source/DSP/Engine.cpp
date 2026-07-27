#include "Engine.h"
#include "Core/MacroMath.h"

namespace vfa::dsp
{

uint64_t Engine::resolveSeed (uint64_t userSeedOrZero, uint64_t autoCounter) noexcept
{
    // 0 = "auto": stable within a prepare/render cycle (ADR-016).
    if (userSeedOrZero == 0)
        return Rng::streamSeed (0x5EEDBA5Eu, (uint32_t) autoCounter, 0, 77);
    return userSeedOrZero;
}

void Engine::prepare (const StreamSpec& spec)
{
    streamSpec = spec;

    dynamics.prepare (spec);
    optical.prepare (spec);
    magnetic.prepare (spec);
    broadcast.prepare (spec);
    generation.prepare (spec);
    transport.prepare (spec);
    delivery.prepare (spec);
    reproduction.prepare (spec);
    noise.prepare (spec);

    for (auto* slot : { &dynSlot, &mediumSlot, &genSlot, &deliverySlot, &reproSlot })
        slot->prepare (spec);

    softeners.resize ((size_t) spec.numChannels);
    for (auto& s : softeners) s.prepare (spec.sampleRate);

    limiterEnv.prepare (spec.sampleRate, 0.1f, 60.0f);
    limiterGain = 1.0f;

    const float smoothHz = 1.0f / (2.0f * kPi * 0.020f); // ~20 ms trims/mix
    inTrimSmooth.setCutoff (smoothHz, spec.sampleRate);
    outTrimSmooth.setCutoff (smoothHz, spec.sampleRate);
    mixSmooth.setCutoff (smoothHz, spec.sampleRate);
    inTrimSmooth.z = 1.0f; outTrimSmooth.z = 1.0f; mixSmooth.z = 1.0f;

    // Auto-gain: 3 s RMS window, 500 ms correction smoothing (DSP_SPEC §11).
    rmsCoeff = 1.0f - std::exp (-1.0f / (3.0f * float (spec.sampleRate)));
    autoGainCoeff = 1.0f - std::exp (-1.0f / (0.5f * float (spec.sampleRate)));
    autoGainValue = 1.0f;
    dryRmsSq = wetRmsSq = 0.0f;

    dryBuffer.setSize (spec.numChannels, spec.maxBlockSize, false, false, true);
    preNoise.setSize (spec.numChannels, spec.maxBlockSize, false, false, true);

    // Dry ring: max latency = transport centre + worst oversampler + margin.
    const int maxLat = (int) std::ceil (transport.latencySamples()
                                        + optical.latencySamples (Quality::high)
                                        + broadcast.latencySamples (Quality::high)) + 16;
    dryDelayLine.setSize (spec.numChannels, std::max (maxLat, 1), false, false, true);
    dryDelayLine.clear();
    dryDelayWrite = 0;

    transition = Transition::stable;
    transitionGain = 1.0f;
    transitionStep = 1.0f / std::max (1.0f, float (spec.sampleRate) * 0.005f);
    activeQuality = Quality::standard;
    activeMedium = Medium::magneticFilm;
    activeLatency = latencySamples (activeQuality, activeMedium);
    dryDelayLength = std::min (activeLatency, dryDelayLine.getNumSamples() - 1);
}

void Engine::reset()
{
    dynamics.reset(); optical.reset(); magnetic.reset(); broadcast.reset();
    generation.reset(); transport.reset(); delivery.reset();
    reproduction.reset(); noise.reset();
    for (auto* slot : { &dynSlot, &mediumSlot, &genSlot, &deliverySlot, &reproSlot })
        slot->reset();
    for (auto& s : softeners) s.reset();
    limiterEnv.reset(); limiterGain = 1.0f;
    dryDelayLine.clear();
    autoGainValue = 1.0f; dryRmsSq = wetRmsSq = 0.0f;
}

void Engine::setSeed (uint64_t userSeedOrZero, uint64_t autoCounter)
{
    resolvedSeed = resolveSeed (userSeedOrZero, autoCounter);
    transport.setSeed (resolvedSeed);
    generation.setSeed (resolvedSeed);
    noise.setSeed (resolvedSeed);
}

int Engine::latencySamples (Quality q, Medium m) const noexcept
{
    float lat = transport.latencySamples();
    switch (m)
    {
        case Medium::opticalMono:
        case Medium::opticalStereo:
        case Medium::kinescope:      lat += optical.latencySamples (q); break;
        case Medium::magneticFilm:
        case Medium::fieldTape:
        case Medium::consumer:       lat += magnetic.latencySamples (q); break;
        case Medium::broadcastMono:
        case Medium::broadcastStereo:lat += broadcast.latencySamples (q); break;
        case Medium::clean:          break;
    }
    return (int) std::ceil (lat);
}

void Engine::processMediumStage (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap)
{
    const int numCh = buffer.getNumChannels();
    const int n = buffer.getNumSamples();
    const float driveLift = GenerationModel::driveLiftDb (snap);

    // Mono media collapse before the model (historically the medium is mono).
    const bool monoMedium = snap.medium == Medium::opticalMono
                         || snap.medium == Medium::kinescope
                         || snap.medium == Medium::broadcastMono;
    if (monoMedium && numCh > 1)
    {
        const float g = 1.0f / std::sqrt (2.0f);
        auto* l = buffer.getWritePointer (0);
        auto* r = buffer.getWritePointer (1);
        for (int i = 0; i < n; ++i)
        {
            const float m = (l[i] + r[i]) * g;
            l[i] = m; r[i] = m;
        }
    }

    switch (snap.medium)
    {
        case Medium::clean: break;
        case Medium::opticalMono:
        case Medium::opticalStereo:
            optical.process (buffer, snap, 1.0f, driveLift);
            break;
        case Medium::kinescope:
            optical.process (buffer, snap, 0.35f, driveLift);
            break;
        case Medium::magneticFilm:
        case Medium::fieldTape:
        case Medium::consumer:
            magnetic.process (buffer, snap, driveLift);
            break;
        case Medium::broadcastMono:
        case Medium::broadcastStereo:
            broadcast.process (buffer, snap, 1.0f);
            break;
    }

    // Common medium behaviors: transient softening and stereo crosstalk.
    if (snap.medium != Medium::clean)
    {
        if (snap.transSoften > 0.001f)
            for (int ch = 0; ch < numCh && ch < (int) softeners.size(); ++ch)
            {
                auto* d = buffer.getWritePointer (ch);
                auto& soft = softeners[(size_t) ch];
                for (int i = 0; i < n; ++i)
                    d[i] = soft.process (d[i], snap.transSoften);
            }

        if (! monoMedium && numCh > 1 && snap.crosstalk > 0.001f)
        {
            // Gain crosstalk approximation (documented, HIST-APPROX):
            // -60 dB..-18 dB bleed as the control rises.
            const float bleed = dbToGain (-60.0f + 42.0f * snap.crosstalk);
            auto* l = buffer.getWritePointer (0);
            auto* r = buffer.getWritePointer (1);
            for (int i = 0; i < n; ++i)
            {
                const float lIn = l[i], rIn = r[i];
                l[i] = lIn + bleed * rIn;
                r[i] = rIn + bleed * lIn;
            }
        }
    }
}

void Engine::applySafetyLimiter (juce::AudioBuffer<float>& buffer, int n)
{
    // Feedback peak limiter into an arctan ceiling at -0.3 dBFS.
    const int numCh = buffer.getNumChannels();
    const float ceiling = dbToGain (-0.3f);
    for (int i = 0; i < n; ++i)
    {
        float peak = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
            peak = std::max (peak, std::abs (buffer.getReadPointer (ch)[i]));
        const float env = limiterEnv.process (peak * limiterGain);
        const float target = env > ceiling ? ceiling / env : 1.0f;
        limiterGain += (target - limiterGain) * (target < limiterGain ? 0.5f : 0.002f);
        limiterGain = std::clamp (limiterGain, 0.05f, 1.0f);
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            float v = d[i] * limiterGain;
            // Soft ceiling: linear to ~0.7, arctan above, bounded < ceiling/0.95.
            const float a = std::abs (v);
            if (a > 0.7f * ceiling)
            {
                const float sign = v > 0 ? 1.0f : -1.0f;
                const float over = (a - 0.7f * ceiling) / ceiling;
                v = sign * ceiling * (0.7f + 0.3f * fastTanh (over / 0.3f));
            }
            d[i] = v;
        }
    }
}

void Engine::process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snapIn)
{
    const int numCh = std::min (buffer.getNumChannels(), streamSpec.numChannels);
    const int n = buffer.getNumSamples();
    if (n == 0 || numCh == 0)
        return;

    // ---- transition machine (quality / medium switches, DEV-004)
    ParamSnapshot snap = snapIn;
    const bool wantsSwitch = snapIn.quality != activeQuality || snapIn.medium != activeMedium;
    if (transition == Transition::stable && wantsSwitch)
        transition = Transition::rampDown;
    snap.quality = activeQuality;
    snap.medium = activeMedium;

    // ---- capture dry input
    for (int ch = 0; ch < numCh; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, n);

    // ---- input trim
    {
        const float target = dbToGain (snap.inTrimDb);
        for (int i = 0; i < n; ++i)
        {
            const float g = inTrimSmooth.process (target);
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] *= g;
        }
    }

    // ---- module chain
    dynSlot.setEnabled (snap.dynOn);
    dynSlot.process (buffer, [&] (juce::AudioBuffer<float>& b) { dynamics.process (b, snap); });

    mediumSlot.setEnabled (snap.mediumOn && snap.medium != Medium::clean);
    mediumSlot.process (buffer, [&] (juce::AudioBuffer<float>& b) { processMediumStage (b, snap); });

    genSlot.setEnabled (snap.genOn && snap.generations > 0.005f);
    genSlot.process (buffer, [&] (juce::AudioBuffer<float>& b) { generation.process (b, snap); });

    // Transport always runs (constant latency); enable only affects depth.
    {
        const float depthScale = (snap.transportOn ? 1.0f : 0.0f)
                               * (snap.genOn ? GenerationModel::wowFlutterScale (snap) : 1.0f);
        transport.process (buffer, snap, depthScale);
    }

    deliverySlot.setEnabled (snap.deliveryOn);
    deliverySlot.process (buffer, [&] (juce::AudioBuffer<float>& b) { delivery.process (b, snap); });

    reproSlot.setEnabled (snap.reproOn);
    reproSlot.process (buffer, [&] (juce::AudioBuffer<float>& b) { reproduction.process (b, snap); });

    // ---- noise (additive)
    const bool artifactsOnly = snap.audition == AuditionMode::artifactsOnly;
    if (artifactsOnly)
        for (int ch = 0; ch < numCh; ++ch)
            preNoise.copyFrom (ch, 0, buffer, ch, 0, n);

    if (snap.noiseOn && snap.audition != AuditionMode::noiseMuted)
    {
        const float lift = snap.genOn ? GenerationModel::noiseLiftDb (snap) : 0.0f;
        noise.process (buffer, snap, lift);
    }

    if (artifactsOnly)
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            const auto* p = preNoise.getReadPointer (ch);
            for (int i = 0; i < n; ++i)
                d[i] -= p[i];
        }

    // ---- transition gain (wet path only)
    if (transition != Transition::stable)
    {
        for (int i = 0; i < n; ++i)
        {
            if (transition == Transition::rampDown)
            {
                transitionGain -= transitionStep;
                if (transitionGain <= 0.0f)
                {
                    transitionGain = 0.0f;
                    // Switch at silence: adopt new quality/medium, reset
                    // rate-dependent stages, update latency alignment.
                    activeQuality = snapIn.quality;
                    activeMedium = snapIn.medium;
                    optical.reset(); magnetic.reset(); broadcast.reset();
                    activeLatency = latencySamples (activeQuality, activeMedium);
                    dryDelayLength = std::min (activeLatency, dryDelayLine.getNumSamples() - 1);
                    transition = Transition::rampUp;
                }
            }
            else if (transition == Transition::rampUp)
            {
                transitionGain += transitionStep;
                if (transitionGain >= 1.0f)
                {
                    transitionGain = 1.0f;
                    transition = Transition::stable;
                }
            }
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] *= transitionGain;
        }
    }

    // ---- safety limiter
    if (snap.safetyLimiter)
        applySafetyLimiter (buffer, n);

    // ---- auto gain (bounded +-6 dB, RMS matched; DSP_SPEC §11)
    {
        float dryAcc = dryRmsSq, wetAcc = wetRmsSq;
        const auto* dl = dryBuffer.getReadPointer (0);
        const auto* wl = buffer.getReadPointer (0);
        for (int i = 0; i < n; ++i)
        {
            dryAcc += rmsCoeff * (dl[i] * dl[i] - dryAcc);
            wetAcc += rmsCoeff * (wl[i] * wl[i] - wetAcc);
        }
        dryRmsSq = flushToZero (dryAcc);
        wetRmsSq = flushToZero (wetAcc);

        float targetGain = 1.0f;
        if (snap.autoGain && wetRmsSq > 1.0e-8f && dryRmsSq > 1.0e-8f)
        {
            targetGain = std::sqrt (dryRmsSq / wetRmsSq);
            targetGain = std::clamp (targetGain, dbToGain (-6.0f), dbToGain (6.0f));
        }
        for (int i = 0; i < n; ++i)
        {
            autoGainValue += autoGainCoeff * (targetGain - autoGainValue);
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] *= autoGainValue;
        }
    }

    // ---- mix with latency-aligned dry, then output trim
    {
        const float mixTarget = snap.mix01;
        const float outTarget = dbToGain (snap.outTrimDb);
        for (int i = 0; i < n; ++i)
        {
            const float m = mixSmooth.process (mixTarget);
            const float og = outTrimSmooth.process (outTarget);
            int readPos = dryDelayWrite - dryDelayLength;
            if (readPos < 0) readPos += dryDelayLine.getNumSamples();
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto* ring = dryDelayLine.getWritePointer (ch);
                ring[dryDelayWrite] = dryBuffer.getReadPointer (ch)[i];
                const float dryDelayed = ring[readPos];
                auto* d = buffer.getWritePointer (ch);
                d[i] = (d[i] * m + dryDelayed * (1.0f - m)) * og;
            }
            if (++dryDelayWrite >= dryDelayLine.getNumSamples())
                dryDelayWrite = 0;
        }
    }
}

} // namespace vfa::dsp
