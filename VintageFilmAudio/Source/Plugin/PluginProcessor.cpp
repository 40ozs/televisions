#include <string_view>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../Parameters/ParameterIDs.h"
#include "../Parameters/EraProfiles.h"
#include "../Presets/FactoryPresets.h"
#include "../DSP/Core/MacroMath.h"

namespace vfa
{
namespace id = vfa::pid;

namespace
{
// Parameters whose change requires a delivery-curve FIR redesign.
const char* const deliveryAffecting[] = {
    id::deliveryCurve, id::delLfRoll, id::delHfRoll, id::delMidShape,
    id::delAcademyAmt, id::delXcurveAmt, id::delPlaybackSize,
    id::fidelity, id::character, id::quality
};
} // namespace

VfaProcessor::VfaProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", params::createParameterLayout())
{
    raw.reserve (params::allParams().size());
    for (const auto& meta : params::allParams())
        raw.push_back (apvts.getRawParameterValue (meta.id));

    // One-time resolution of the named audio-thread pointer cache (F8).
    {
        auto r = [this] (const char* pid) { return apvts.getRawParameterValue (pid); };
        rp = { r (id::era), r (id::medium), r (id::condition), r (id::character),
               r (id::fidelity), r (id::generation), r (id::genInteger),
               r (id::genVariability), r (id::artifacts), r (id::noiseMacro),
               r (id::deliveryCurve), r (id::mix), r (id::inTrim), r (id::outTrim),
               r (id::autoGain), r (id::safetyLimiter), r (id::audition),
               r (id::quality), r (id::seed),
               r (id::deliveryOn), r (id::mediumOn), r (id::transportOn),
               r (id::genOn), r (id::noiseOn), r (id::dynOn), r (id::reproOn),
               r (id::delLfRoll), r (id::delHfRoll), r (id::delMidShape),
               r (id::delAcademyAmt), r (id::delXcurveAmt), r (id::delPlaybackSize),
               r (id::medOpticalDist), r (id::medOpticalMode), r (id::medImageSpread),
               r (id::medMagSat), r (id::medTapeSpeed), r (id::medHeadBump),
               r (id::medTransSoften), r (id::medCrosstalk),
               r (id::wow), r (id::wowRate), r (id::flutter), r (id::flutterRate),
               r (id::drift), r (id::scrape), r (id::stereoLink), r (id::transportQuality),
               r (id::nsHiss), r (id::nsCell), r (id::nsBroadcast), r (id::nsHum),
               r (id::nsHumFreq), r (id::nsHumHarm), r (id::nsBuzz), r (id::nsCrackle),
               r (id::nsDirt), r (id::nsDropout), r (id::nsProjector),
               r (id::nsPrintThrough), r (id::nsDuck), r (id::nsSilence), r (id::nsWidth),
               r (id::dynAgc), r (id::dynComp), r (id::dynLimit), r (id::dynRelChar),
               r (id::dynDialog), r (id::dynPump),
               r (id::reproMono), r (id::monoLaw), r (id::reproWidth), r (id::reproSpeaker) };
    }

    defaultState = apvts.copyState().createCopy();
    defaultState.setProperty (state::kVersionProperty, state::kStateVersion, nullptr);

    for (const auto* pid : deliveryAffecting)
        apvts.addParameterListener (pid, this);

    // Message-thread polling for redesigns and latency reports (RT-safe:
    // the audio thread never posts messages). Runs for the processor's
    // lifetime; each tick is a couple of atomic loads when idle.
    startTimer (50);
}

VfaProcessor::~VfaProcessor()
{
    for (const auto* pid : deliveryAffecting)
        apvts.removeParameterListener (pid, this);
}

std::atomic<float>* VfaProcessor::rawFor (const char* pid) const noexcept
{
    const auto& all = params::allParams();
    for (size_t i = 0; i < all.size(); ++i)
        if (std::string_view (all[i].id) == pid)
            return raw[i];
    jassertfalse;
    return nullptr;
}

dsp::ParamSnapshot VfaProcessor::buildSnapshot() const noexcept
{
    using namespace dsp;
    ParamSnapshot s;
    // Direct relaxed loads through the constructor-resolved pointer cache —
    // no lookups on the audio thread (F8).
    auto get = [] (const std::atomic<float>* p) noexcept { return p->load (std::memory_order_relaxed); };

    macros::MacroValues mv;
    mv.character = get (rp.character);
    mv.fidelity  = get (rp.fidelity);
    mv.artifacts = get (rp.artifacts);
    mv.noise     = get (rp.noiseMacro);

    s.inTrimDb = get (rp.inTrim);
    s.outTrimDb = get (rp.outTrim);
    s.mix01 = get (rp.mix) * 0.01f;
    s.autoGain = get (rp.autoGain) >= 0.5f;
    s.safetyLimiter = get (rp.safetyLimiter) >= 0.5f;
    s.audition = (AuditionMode) (int) get (rp.audition);
    s.quality = (Quality) (int) get (rp.quality);
    s.seed = dsp::Engine::resolveSeed ((uint64_t) get (rp.seed), autoSeedCounter);

    s.deliveryOn = get (rp.deliveryOn) >= 0.5f;
    s.mediumOn = get (rp.mediumOn) >= 0.5f;
    s.transportOn = get (rp.transportOn) >= 0.5f;
    s.genOn = get (rp.genOn) >= 0.5f;
    s.noiseOn = get (rp.noiseOn) >= 0.5f;
    s.dynOn = get (rp.dynOn) >= 0.5f;
    s.reproOn = get (rp.reproOn) >= 0.5f;

    s.era = (Era) (int) get (rp.era);
    s.medium = (Medium) (int) get (rp.medium);
    s.condition = (Condition) (int) get (rp.condition);
    s.curveMode = (CurveMode) (int) get (rp.deliveryCurve);

    s.delLfRollHz = macros::effectiveLfRollHz (get (rp.delLfRoll), mv.fidelity);
    s.delHfRollHz = macros::effectiveHfRollHz (get (rp.delHfRoll), mv.fidelity);
    s.delMidShapeDb = macros::effectiveMidShapeDb (get (rp.delMidShape), mv.character);
    s.academyAmt = get (rp.delAcademyAmt);
    s.xcurveAmt = get (rp.delXcurveAmt);
    s.playbackSize = get (rp.delPlaybackSize);

    s.opticalDist = macros::effectiveDistortion (get (rp.medOpticalDist), mv.fidelity);
    s.imageSpread = get (rp.medImageSpread);
    s.magSat = macros::effectiveDistortion (get (rp.medMagSat), mv.fidelity);
    s.headBump = get (rp.medHeadBump);
    s.transSoften = macros::effectiveTransientSoften (get (rp.medTransSoften), mv.character, mv.fidelity);
    s.crosstalk = get (rp.medCrosstalk);
    s.opticalMode = (OpticalMode) (int) get (rp.medOpticalMode);
    s.tapeSpeed = (TapeSpeed) (int) get (rp.medTapeSpeed);

    s.wow = get (rp.wow);
    s.wowRateHz = get (rp.wowRate);
    s.flutter = get (rp.flutter);
    s.flutterRateHz = get (rp.flutterRate);
    s.drift = get (rp.drift);
    s.scrape = get (rp.scrape);
    s.stereoLink = get (rp.stereoLink) >= 0.5f;
    s.transportQuality = (TransportQuality) (int) get (rp.transportQuality);

    s.generations = get (rp.generation);
    s.genInteger = get (rp.genInteger) >= 0.5f;
    s.genVariability = get (rp.genVariability);

    s.nsHiss = macros::effectiveNoiseAmount (get (rp.nsHiss), mv.noise, mv.fidelity);
    s.nsCell = macros::effectiveNoiseAmount (get (rp.nsCell), mv.noise, mv.fidelity);
    s.nsBroadcast = macros::effectiveNoiseAmount (get (rp.nsBroadcast), mv.noise, mv.fidelity);
    s.nsHum = macros::effectiveNoiseAmount (get (rp.nsHum), mv.noise, mv.fidelity);
    s.nsHumHarm = get (rp.nsHumHarm);
    s.nsBuzz = macros::effectiveNoiseAmount (get (rp.nsBuzz), mv.noise, mv.fidelity);
    s.nsCrackle = macros::effectiveArtifactAmount (get (rp.nsCrackle), mv.artifacts);
    s.nsDirt = macros::effectiveArtifactAmount (get (rp.nsDirt), mv.artifacts);
    s.nsDropout = macros::effectiveArtifactAmount (get (rp.nsDropout), mv.artifacts);
    s.nsProjector = macros::effectiveArtifactAmount (get (rp.nsProjector), mv.artifacts);
    s.nsPrintThrough = macros::effectiveArtifactAmount (get (rp.nsPrintThrough), mv.artifacts);
    s.nsDuck = get (rp.nsDuck);
    s.nsWidth = get (rp.nsWidth);
    s.nsSilence = get (rp.nsSilence) >= 0.5f;
    s.humFreqHz = ((int) get (rp.nsHumFreq)) == 0 ? 50.0f : 60.0f;

    s.agc = get (rp.dynAgc);
    s.comp = macros::effectiveCompression (get (rp.dynComp), mv.character);
    s.limit = get (rp.dynLimit);
    s.relChar = get (rp.dynRelChar);
    s.dialog = get (rp.dynDialog);
    s.pump = get (rp.dynPump);

    s.monoMode = (MonoMode) (int) get (rp.reproMono);
    s.monoLaw = (MonoLaw) (int) get (rp.monoLaw);
    s.width = get (rp.reproWidth);
    s.smallSpeaker = get (rp.reproSpeaker);

    return s;
}

bool VfaProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void VfaProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Block the redesign timer for the whole reconfiguration; also drop the
    // prepared flag so a timer tick that raced past the mutex acquisition
    // order cannot design into buffers that are being reallocated.
    enginePrepared.store (false, std::memory_order_release);
    const std::lock_guard<std::mutex> lock (configMutex);

    const dsp::StreamSpec spec { sampleRate,
                                 std::max (samplesPerBlock, 16),
                                 std::max (getTotalNumOutputChannels(), 1) };
    engine.prepare (spec);

    ++autoSeedCounter;
    const auto seedParam = (uint64_t) rawFor (id::seed)->load();
    engine.setSeed (seedParam, autoSeedCounter);
    lastSeedParam = (float) seedParam;

    const auto snap = buildSnapshot();
    engine.designDeliveryNow (snap);
    engine.primeActive (snap);
    setLatencySamples (engine.currentLatencySamples());
    latencyToReport.store (engine.currentLatencySamples());
    lastReportedLatency = engine.currentLatencySamples();
    enginePrepared.store (true, std::memory_order_release);
}

void VfaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const auto snap = buildSnapshot();

    // Seed changes applied at block boundaries on the audio thread (safe:
    // same thread owns the rng state; setSeed is allocation-free).
    const float seedNow = rp.seed->load (std::memory_order_relaxed);
    if (seedNow != lastSeedParam)
    {
        lastSeedParam = seedNow;
        engine.setSeed ((uint64_t) seedNow, autoSeedCounter);
    }

    engine.process (buffer, snap);

    latencyToReport.store (engine.currentLatencySamples(), std::memory_order_relaxed);
}

void VfaProcessor::parameterChanged (const juce::String&, float)
{
    deliveryDirty.store (true, std::memory_order_release);
}

void VfaProcessor::timerCallback()
{
    // Nothing to redesign or report before the engine has been prepared —
    // the timer starts at construction and must not race prepareToPlay.
    // try_lock (never block the message thread): if a prepare is running,
    // simply retry on the next tick.
    if (! enginePrepared.load (std::memory_order_acquire))
        return;
    const std::unique_lock<std::mutex> lock (configMutex, std::try_to_lock);
    if (! lock.owns_lock() || ! enginePrepared.load (std::memory_order_acquire))
        return;

    const int lat = latencyToReport.load (std::memory_order_relaxed);
    if (lat != lastReportedLatency)
    {
        lastReportedLatency = lat;
        setLatencySamples (lat);
    }

    if (deliveryDirty.exchange (false))
        if (! engine.requestDeliveryRedesign (buildSnapshot()))
            deliveryDirty.store (true);   // staging busy: retry next tick
}

void VfaProcessor::setParamPlain (const juce::String& paramID, float plainValue)
{
    if (auto* p = apvts.getParameter (paramID))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

void VfaProcessor::applyEraProfile()
{
    const auto snap = buildSnapshot();
    for (const auto& [pid, value] : profiles::profileFor (snap.era, snap.medium, snap.condition))
        setParamPlain (pid, value);
}

// ------------------------------------------------------------------ programs
int VfaProcessor::getNumPrograms()
{
    return (int) presets::factoryPresets().size();
}

const juce::String VfaProcessor::getProgramName (int index)
{
    const auto& list = presets::factoryPresets();
    if (index >= 0 && index < (int) list.size())
        return list[(size_t) index].name;
    return {};
}

void VfaProcessor::setCurrentProgram (int index)
{
    const auto& list = presets::factoryPresets();
    if (index < 0 || index >= (int) list.size())
        return;
    currentProgram = index;

    // Single pass: each parameter is written exactly once with its final
    // value (preset override if present, else default). A reset-then-apply
    // sequence would let the audio thread snapshot a half-applied state
    // between the two passes (review finding F6).
    const auto& values = list[(size_t) index].values;
    for (const auto& meta : params::allParams())
    {
        float target = meta.def;
        for (const auto& [pid, value] : values)
            if (std::string_view (pid) == meta.id) { target = value; break; }
        setParamPlain (meta.id, target);
    }
}

// --------------------------------------------------------------------- state
void VfaProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (state::kVersionProperty, state::kStateVersion, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void VfaProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
    {
        // Corrupted/foreign state: fall back to defaults, never crash.
        apvts.replaceState (defaultState.createCopy());
        stateWasCorrupted.store (true);
        return;
    }

    auto tree = juce::ValueTree::fromXml (*xml);
    const auto result = state::migrateInPlace (tree);
    if (result == state::LoadResult::corrupted)
    {
        apvts.replaceState (defaultState.createCopy());
        stateWasCorrupted.store (true);
        return;
    }

    stateWasCorrupted.store (false);
    apvts.replaceState (tree);
    deliveryDirty.store (true);   // timer picks this up on the message thread
}

juce::AudioProcessorEditor* VfaProcessor::createEditor()
{
    return new VfaEditor (*this);
}

} // namespace vfa

// JUCE plugin entry point.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new vfa::VfaProcessor();
}
