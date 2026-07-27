#pragma once

#include "PluginProcessor.h"

namespace vfa
{

// Phase-1 editor: generic parameter panel so every control is reachable and
// automatable from day one. Replaced by the custom era/macro editor in
// Phase 7 (see Docs/IMPLEMENTATION_PLAN.md).
class VfaEditor : public juce::AudioProcessorEditor
{
public:
    explicit VfaEditor (VfaProcessor&);
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    VfaProcessor& processor;
    juce::GenericAudioProcessorEditor generic;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VfaEditor)
};

} // namespace vfa
