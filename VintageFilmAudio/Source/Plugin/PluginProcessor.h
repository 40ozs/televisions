#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../DSP/Engine.h"
#include "../Parameters/ParameterLayout.h"
#include "../Parameters/StateMigration.h"

namespace vfa
{

class VfaProcessor : public juce::AudioProcessor,
                     private juce::AudioProcessorValueTreeState::Listener,
                     private juce::AsyncUpdater,
                     private juce::Timer
{
public:
    VfaProcessor();
    ~VfaProcessor() override;

    // -- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Vintage Film and TV"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // -- project API
    juce::AudioProcessorValueTreeState& parameters() noexcept { return apvts; }
    dsp::ParamSnapshot buildSnapshot() const noexcept;

    // Apply an Era/Medium/Condition profile as an explicit user gesture
    // (message thread; ADR-003). Writes base params via the host-visible path.
    void applyEraProfile();

    bool lastStateLoadWasCorrupted() const noexcept { return stateWasCorrupted.load(); }
    float gainReductionDb() const noexcept { return engine.gainReductionDb(); }
    const dsp::Engine& dspEngine() const noexcept { return engine; }

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void timerCallback() override;
    void setParamPlain (const juce::String& paramID, float plainValue);

    juce::AudioProcessorValueTreeState apvts;
    dsp::Engine engine;

    // Cached raw-value pointers, ordered like params::allParams().
    std::vector<std::atomic<float>*> raw;
    std::atomic<float>* rawFor (const char* id) const noexcept;

    juce::ValueTree defaultState;
    std::atomic<bool> stateWasCorrupted { false };
    std::atomic<bool> deliveryDirty { false };
    std::atomic<int> latencyToReport { 0 };

    int currentProgram = 0;
    uint64_t autoSeedCounter = 1;
    float lastSeedParam = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VfaProcessor)
};

} // namespace vfa
