#include "../Harness/VfaTest.h"
#include "Plugin/PluginProcessor.h"
#include "Parameters/ParameterIDs.h"
#include "DSP/Core/Rng.h"

// State round-trip integrity (TEST_PLAN §State).

VFA_TEST (State_save_load_roundtrip_bit_exact)
{
    vfa::VfaProcessor a;

    // Randomize every parameter deterministically via normalized values.
    vfa::dsp::Rng rng (2024);
    std::vector<float> normValues;
    for (const auto& meta : vfa::params::allParams())
    {
        auto* p = a.parameters().getParameter (meta.id);
        REQUIRE (p != nullptr);
        const float nv = rng.next01();
        p->setValueNotifyingHost (nv);
        // Snap through the plain-value domain: discrete parameters (bool/
        // choice/int) quantize on state round-trip, floats stay exact.
        normValues.push_back (p->convertTo0to1 (p->convertFrom0to1 (p->getValue())));
    }

    juce::MemoryBlock block;
    a.getStateInformation (block);
    CHECK (block.getSize() > 0);

    vfa::VfaProcessor b;
    b.setStateInformation (block.getData(), (int) block.getSize());
    CHECK (! b.lastStateLoadWasCorrupted());

    size_t i = 0;
    for (const auto& meta : vfa::params::allParams())
    {
        auto* p = b.parameters().getParameter (meta.id);
        REQUIRE (p != nullptr);
        char msg[128];
        std::snprintf (msg, sizeof (msg), "%s: %.6f vs %.6f", meta.id, p->getValue(), normValues[i]);
        CHECK_MSG (std::abs (p->getValue() - normValues[i]) < 1.0e-5f, msg);
        ++i;
    }
}

VFA_TEST (State_version_property_present)
{
    vfa::VfaProcessor a;
    juce::MemoryBlock block;
    a.getStateInformation (block);

    const auto xml = juce::AudioProcessor::getXmlFromBinary (block.getData(), (int) block.getSize());
    REQUIRE (xml != nullptr);
    const auto tree = juce::ValueTree::fromXml (*xml);
    CHECK ((int) tree.getProperty (vfa::state::kVersionProperty) == vfa::state::kStateVersion);
}
