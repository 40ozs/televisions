#include "../Harness/VfaTest.h"
#include "Plugin/PluginProcessor.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/StateMigration.h"

// State robustness (ADR-011): corrupted input, unknown params, missing
// params, out-of-range values — never crash, always land in a valid state.

VFA_TEST (Migration_corrupted_binary_falls_back_to_defaults)
{
    vfa::VfaProcessor proc;
    // Move a parameter away from default first.
    auto* p = proc.parameters().getParameter (vfa::pid::wow);
    REQUIRE (p != nullptr);
    p->setValueNotifyingHost (1.0f);

    const char garbage[] = "\x13\x37not-xml-at-all\x7F\x42";
    proc.setStateInformation (garbage, (int) sizeof (garbage));
    CHECK (proc.lastStateLoadWasCorrupted());
    // Back at default.
    const float plain = p->convertFrom0to1 (p->getValue());
    CHECK_NEAR (plain, 0.25f, 1.0e-3);
}

VFA_TEST (Migration_wrong_root_type_rejected_safely)
{
    vfa::VfaProcessor proc;
    juce::ValueTree wrong ("NOT_PARAMS");
    wrong.setProperty ("foo", 1, nullptr);
    juce::MemoryBlock block;
    if (auto xml = wrong.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, block);
    proc.setStateInformation (block.getData(), (int) block.getSize());
    CHECK (proc.lastStateLoadWasCorrupted());
}

VFA_TEST (Migration_unknown_param_ignored_missing_defaulted)
{
    // A v1 state with one out-of-range known value and one unknown parameter:
    // loading must clamp the former, ignore the latter, default the rest.
    vfa::VfaProcessor donor;
    auto state = donor.parameters().copyState();

    auto known = state.getChildWithProperty ("id", juce::String (vfa::pid::mix));
    if (known.isValid())
        known.setProperty ("value", 400.0f, nullptr);   // out of range 0..100

    juce::ValueTree unknown ("PARAM");
    unknown.setProperty ("id", "someFutureParam", nullptr);
    unknown.setProperty ("value", 0.5f, nullptr);
    state.appendChild (unknown, nullptr);
    state.setProperty (vfa::state::kVersionProperty, 1, nullptr);

    juce::MemoryBlock block;
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, block);

    vfa::VfaProcessor proc;
    proc.setStateInformation (block.getData(), (int) block.getSize());
    CHECK (! proc.lastStateLoadWasCorrupted());

    auto* mix = proc.parameters().getParameter (vfa::pid::mix);
    REQUIRE (mix != nullptr);
    const float plain = mix->convertFrom0to1 (mix->getValue());
    CHECK_MSG (plain <= 100.0f + 1.0e-3f, "out-of-range value not clamped");

    auto* wow = proc.parameters().getParameter (vfa::pid::wow);
    REQUIRE (wow != nullptr);
    CHECK_NEAR (wow->convertFrom0to1 (wow->getValue()), 0.25f, 1.0e-3);
}

VFA_TEST (Migration_nonfinite_value_sanitized)
{
    vfa::VfaProcessor donor;
    auto state = donor.parameters().copyState();
    auto child = state.getChildWithProperty ("id", juce::String (vfa::pid::outTrim));
    if (child.isValid())
        child.setProperty ("value", std::numeric_limits<float>::quiet_NaN(), nullptr);
    state.setProperty (vfa::state::kVersionProperty, 1, nullptr);

    // Run the migration directly (XML would stringify the NaN differently).
    const auto result = vfa::state::migrateInPlace (state);
    CHECK (result != vfa::state::LoadResult::corrupted);
    auto after = state.getChildWithProperty ("id", juce::String (vfa::pid::outTrim));
    const float v = (float) after.getProperty ("value");
    CHECK (std::isfinite (v));
    CHECK (v >= -24.0f && v <= 24.0f);
}

VFA_TEST (Migration_future_version_tolerated)
{
    vfa::VfaProcessor donor;
    auto state = donor.parameters().copyState();
    state.setProperty (vfa::state::kVersionProperty, 99, nullptr);
    juce::MemoryBlock block;
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, block);

    vfa::VfaProcessor proc;
    proc.setStateInformation (block.getData(), (int) block.getSize());
    CHECK (! proc.lastStateLoadWasCorrupted());
}
