#pragma once

#include "PluginProcessor.h"
#include "../UI/CurveDisplay.h"
#include "../UI/LevelMeter.h"

namespace vfa
{

// Era/Medium/Condition-first editor (task brief §12):
//   header (title / preset / A-B) → context row (era, medium, condition) →
//   macro row → chain overview with module enables → advanced tabs →
//   meters + response display + mix/output.
// All controls attach to APVTS; era/medium/condition changes from THIS UI
// additionally apply the profile as an explicit user gesture (ADR-003).
// Resizable, minimum 13-inch-laptop friendly, no audio-rate repaints.
class VfaEditor : public juce::AudioProcessorEditor
{
public:
    explicit VfaEditor (VfaProcessor&);
    ~VfaEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct LabelledSlider
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void addRotary (LabelledSlider& s, const char* paramID, const juce::String& text,
                    juce::Component& parent);
    void addCombo (juce::ComboBox& box, std::unique_ptr<ComboAttachment>& att,
                   const char* paramID);
    void applyProfileFromUi();
    void snapshotToSlot (juce::MemoryBlock& slot);

    VfaProcessor& processor;

    // Header
    juce::Label title;
    juce::ComboBox presetBox;
    juce::TextButton abButton { "A/B" }, copyButton { "Copy" };
    juce::MemoryBlock slotA, slotB;
    bool usingSlotA = true;

    // Context row
    juce::ComboBox eraBox, mediumBox, conditionBox;
    std::unique_ptr<ComboAttachment> eraAtt, mediumAtt, conditionAtt;
    juce::Label eraLabel, mediumLabel, conditionLabel;

    // Macro row
    LabelledSlider character, fidelity, generation, artifacts, noiseMacro;

    // Chain overview (module enables in signal order)
    struct ChainCell
    {
        juce::ToggleButton toggle;
        std::unique_ptr<ButtonAttachment> attachment;
    };
    std::array<ChainCell, 7> chain;
    juce::Label chainLabel;

    // Advanced tabs
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
    juce::Component deliveryTab, mediumTab, transportTab, noiseTab, dynamicsTab, outputTab;
    std::vector<std::unique_ptr<LabelledSlider>> tabSliders;
    juce::ComboBox curveBox, opticalModeBox, tapeSpeedBox, humFreqBox,
                   monoModeBox, monoLawBox, transportQualityBox, qualityBox, auditionBox;
    std::vector<std::unique_ptr<ComboAttachment>> tabCombos;
    juce::ToggleButton stereoLinkToggle { "Stereo Link" }, genIntegerToggle { "Exact Gens" },
                       nsSilenceToggle { "Noise In Silence" }, autoGainToggle { "Auto Gain" },
                       safetyToggle { "Safety Limiter" };
    std::vector<std::unique_ptr<ButtonAttachment>> tabButtons;

    // Footer
    ui::CurveDisplay curveDisplay;
    ui::LevelMeter inMeter, outMeter, grMeter;
    LabelledSlider mix, outTrim;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VfaEditor)
};

} // namespace vfa
