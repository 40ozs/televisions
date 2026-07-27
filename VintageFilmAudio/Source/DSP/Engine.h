#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "Core/StreamSpec.h"
#include "Core/ParamSnapshot.h"
#include "Core/ModuleSlot.h"
#include "Core/Filters.h"
#include "DeliveryCurves/DeliveryCurveModule.h"
#include "Optical/OpticalModel.h"
#include "Magnetic/MagneticModel.h"
#include "Broadcast/BroadcastChain.h"
#include "Transport/WowFlutterEngine.h"
#include "GenerationLoss/GenerationModel.h"
#include "Noise/NoiseArtifactEngine.h"
#include "Dynamics/PeriodDynamics.h"
#include "Reproduction/Reproduction.h"

namespace vfa::dsp
{

// Full processing chain (SAS §2):
//   InputTrim -> PeriodDynamics -> Medium (optical/magnetic/broadcast +
//   common crosstalk/softening/mono-collapse) -> GenerationLoss -> Transport
//   -> DeliveryCurve -> Reproduction -> Noise (additive) -> SafetyLimiter ->
//   AutoGain -> Mix (latency-aligned dry) -> OutputTrim.
//
// Real-time contract: no allocation, locks, or file I/O in process();
// everything sized in prepare(). Quality/medium switches ramp the wet path
// through a 5 ms fade-down/switch/fade-up (DEV-004) — bounded, click-free.
class Engine
{
public:
    void prepare (const StreamSpec& spec);
    void reset();

    // Seed resolution (ADR-016): called at prepare and when the seed
    // parameter changes (message thread; audio not touching modules' rng
    // state concurrently is guaranteed by only reseeding inside prepare or
    // via the processor's suspend/param mechanism).
    void setSeed (uint64_t userSeedOrZero, uint64_t autoCounter);

    // Adopt the snapshot's quality/medium immediately (call after prepare,
    // before audio runs) so playback does not start inside a transition ramp.
    void primeActive (const ParamSnapshot& snap) noexcept
    {
        activeQuality = snap.quality;
        activeMedium = snap.medium;
        activeLatency = latencySamples (activeQuality, activeMedium);
        dryDelayLength = std::min (activeLatency, dryDelayLine.getNumSamples() - 1);
        transition = Transition::stable;
        transitionGain = 1.0f;
    }

    void process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap);

    // Delivery-curve redesign passthroughs (message thread).
    void designDeliveryNow (const ParamSnapshot& snap) { delivery.designNow (snap); }
    bool requestDeliveryRedesign (const ParamSnapshot& snap) { return delivery.requestRedesign (snap); }
    bool isDeliveryStagingPending() const noexcept { return delivery.isStagingPending(); }

    // Total wet-path latency for the given settings (samples, integer).
    int latencySamples (Quality q, Medium m) const noexcept;
    int currentLatencySamples() const noexcept { return activeLatency; }

    float gainReductionDb() const noexcept { return dynamics.currentGainReductionDb(); }

    static uint64_t resolveSeed (uint64_t userSeedOrZero, uint64_t autoCounter) noexcept;

private:
    void processMediumStage (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap);
    void applySafetyLimiter (juce::AudioBuffer<float>& buffer, int numSamples);

    StreamSpec streamSpec;

    // Modules
    PeriodDynamics dynamics;
    OpticalModel optical;
    MagneticModel magnetic;
    BroadcastChain broadcast;
    GenerationModel generation;
    WowFlutterEngine transport;
    DeliveryCurveModule delivery;
    Reproduction reproduction;
    NoiseArtifactEngine noise;

    // Slots (click-free enables)
    ModuleSlot dynSlot, mediumSlot, genSlot, deliverySlot, reproSlot;

    // Medium-stage helpers
    std::vector<TransientSoftener> softeners;

    // Output stage
    EnvelopeFollower limiterEnv;
    float limiterGain = 1.0f;
    OnePoleLP inTrimSmooth, outTrimSmooth, mixSmooth;
    float autoGainValue = 1.0f;
    float dryRmsSq = 0.0f, wetRmsSq = 0.0f;
    float rmsCoeff = 0.0f, autoGainCoeff = 0.0f;

    // Dry path (latency-aligned for mix)
    juce::AudioBuffer<float> dryBuffer;      // this block's input
    juce::AudioBuffer<float> dryDelayLine;   // ring buffer, maxLatency
    int dryDelayWrite = 0, dryDelayLength = 0;
    juce::AudioBuffer<float> preNoise;       // for artifacts-only audition

    // Quality/medium transition machine (DEV-004)
    enum class Transition { stable, rampDown, rampUp };
    Transition transition = Transition::stable;
    float transitionGain = 1.0f, transitionStep = 0.01f;
    Quality activeQuality = Quality::standard;
    Medium activeMedium = Medium::magneticFilm;
    int activeLatency = 0;

    uint64_t resolvedSeed = 1;
};

} // namespace vfa::dsp
