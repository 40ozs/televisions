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

    defaultState = apvts.copyState().createCopy();
    defaultState.setProperty (state::kVersionProperty, state::kStateVersion, nullptr);

    for (const auto* pid : deliveryAffecting)
        apvts.addParameterListener (pid, this);
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
    auto get = [this] (const char* pid) noexcept { auto* p = rawFor (pid); return p ? p->load (std::memory_order_relaxed) : 0.0f; };

    macros::MacroValues mv;
    mv.character = get (id::character);
    mv.fidelity  = get (id::fidelity);
    mv.artifacts = get (id::artifacts);
    mv.noise     = get (id::noiseMacro);

    s.inTrimDb = get (id::inTrim);
    s.outTrimDb = get (id::outTrim);
    s.mix01 = get (id::mix) * 0.01f;
    s.autoGain = get (id::autoGain) >= 0.5f;
    s.safetyLimiter = get (id::safetyLimiter) >= 0.5f;
    s.audition = (AuditionMode) (int) get (id::audition);
    s.quality = (Quality) (int) get (id::quality);
    s.seed = dsp::Engine::resolveSeed ((uint64_t) get (id::seed), autoSeedCounter);

    s.deliveryOn = get (id::deliveryOn) >= 0.5f;
    s.mediumOn = get (id::mediumOn) >= 0.5f;
    s.transportOn = get (id::transportOn) >= 0.5f;
    s.genOn = get (id::genOn) >= 0.5f;
    s.noiseOn = get (id::noiseOn) >= 0.5f;
    s.dynOn = get (id::dynOn) >= 0.5f;
    s.reproOn = get (id::reproOn) >= 0.5f;

    s.era = (Era) (int) get (id::era);
    s.medium = (Medium) (int) get (id::medium);
    s.condition = (Condition) (int) get (id::condition);
    s.curveMode = (CurveMode) (int) get (id::deliveryCurve);

    s.delLfRollHz = macros::effectiveLfRollHz (get (id::delLfRoll), mv.fidelity);
    s.delHfRollHz = macros::effectiveHfRollHz (get (id::delHfRoll), mv.fidelity);
    s.delMidShapeDb = macros::effectiveMidShapeDb (get (id::delMidShape), mv.character);
    s.academyAmt = get (id::delAcademyAmt);
    s.xcurveAmt = get (id::delXcurveAmt);
    s.playbackSize = get (id::delPlaybackSize);

    s.opticalDist = macros::effectiveDistortion (get (id::medOpticalDist), mv.fidelity);
    s.imageSpread = get (id::medImageSpread);
    s.magSat = macros::effectiveDistortion (get (id::medMagSat), mv.fidelity);
    s.headBump = get (id::medHeadBump);
    s.transSoften = macros::effectiveTransientSoften (get (id::medTransSoften), mv.character, mv.fidelity);
    s.crosstalk = get (id::medCrosstalk);
    s.opticalMode = (OpticalMode) (int) get (id::medOpticalMode);
    s.tapeSpeed = (TapeSpeed) (int) get (id::medTapeSpeed);

    s.wow = get (id::wow);
    s.wowRateHz = get (id::wowRate);
    s.flutter = get (id::flutter);
    s.flutterRateHz = get (id::flutterRate);
    s.drift = get (id::drift);
    s.scrape = get (id::scrape);
    s.stereoLink = get (id::stereoLink) >= 0.5f;
    s.transportQuality = (TransportQuality) (int) get (id::transportQuality);

    s.generations = get (id::generation);
    s.genInteger = get (id::genInteger) >= 0.5f;
    s.genVariability = get (id::genVariability);

    s.nsHiss = macros::effectiveNoiseAmount (get (id::nsHiss), mv.noise, mv.fidelity);
    s.nsCell = macros::effectiveNoiseAmount (get (id::nsCell), mv.noise, mv.fidelity);
    s.nsBroadcast = macros::effectiveNoiseAmount (get (id::nsBroadcast), mv.noise, mv.fidelity);
    s.nsHum = macros::effectiveNoiseAmount (get (id::nsHum), mv.noise, mv.fidelity);
    s.nsHumHarm = get (id::nsHumHarm);
    s.nsBuzz = macros::effectiveNoiseAmount (get (id::nsBuzz), mv.noise, mv.fidelity);
    s.nsCrackle = macros::effectiveArtifactAmount (get (id::nsCrackle), mv.artifacts);
    s.nsDirt = macros::effectiveArtifactAmount (get (id::nsDirt), mv.artifacts);
    s.nsDropout = macros::effectiveArtifactAmount (get (id::nsDropout), mv.artifacts);
    s.nsProjector = macros::effectiveArtifactAmount (get (id::nsProjector), mv.artifacts);
    s.nsPrintThrough = macros::effectiveArtifactAmount (get (id::nsPrintThrough), mv.artifacts);
    s.nsDuck = get (id::nsDuck);
    s.nsWidth = get (id::nsWidth);
    s.nsSilence = get (id::nsSilence) >= 0.5f;
    s.humFreqHz = ((int) get (id::nsHumFreq)) == 0 ? 50.0f : 60.0f;

    s.agc = get (id::dynAgc);
    s.comp = macros::effectiveCompression (get (id::dynComp), mv.character);
    s.limit = get (id::dynLimit);
    s.relChar = get (id::dynRelChar);
    s.dialog = get (id::dynDialog);
    s.pump = get (id::dynPump);

    s.monoMode = (MonoMode) (int) get (id::reproMono);
    s.monoLaw = (MonoLaw) (int) get (id::monoLaw);
    s.width = get (id::reproWidth);
    s.smallSpeaker = get (id::reproSpeaker);

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
}

void VfaProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const auto snap = buildSnapshot();

    // Seed changes applied at block boundaries on the audio thread (safe:
    // same thread owns the rng state; setSeed is allocation-free).
    const float seedNow = rawFor (id::seed)->load (std::memory_order_relaxed);
    if (seedNow != lastSeedParam)
    {
        lastSeedParam = seedNow;
        engine.setSeed ((uint64_t) seedNow, autoSeedCounter);
    }

    engine.process (buffer, snap);

    const int lat = engine.currentLatencySamples();
    if (lat != latencyToReport.load (std::memory_order_relaxed))
    {
        latencyToReport.store (lat);
        triggerAsyncUpdate();
    }
}

void VfaProcessor::parameterChanged (const juce::String&, float)
{
    deliveryDirty.store (true, std::memory_order_release);
    triggerAsyncUpdate();
}

void VfaProcessor::handleAsyncUpdate()
{
    setLatencySamples (latencyToReport.load());

    if (deliveryDirty.exchange (false))
    {
        if (! engine.requestDeliveryRedesign (buildSnapshot()))
        {
            deliveryDirty.store (true);
            startTimer (50);   // retry until the audio thread consumes staging
            return;
        }
    }
    stopTimer();
}

void VfaProcessor::timerCallback()
{
    handleAsyncUpdate();
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

    // Reset to defaults, then apply the preset writes.
    for (const auto& meta : params::allParams())
        setParamPlain (meta.id, meta.type == params::ParamMeta::Type::Choice
                                    || meta.type == params::ParamMeta::Type::Bool
                                    || meta.type == params::ParamMeta::Type::Int
                                ? meta.def
                                : meta.def);
    for (const auto& [pid, value] : list[(size_t) index].values)
        setParamPlain (pid, value);
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
    deliveryDirty.store (true);
    triggerAsyncUpdate();
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
