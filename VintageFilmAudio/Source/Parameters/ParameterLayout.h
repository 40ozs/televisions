#pragma once

#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>

namespace vfa::params
{

// In-code mirror of Docs/PARAMETER_MANIFEST.md. The APVTS layout is built
// from this table and ParameterManifestTest cross-checks the table against
// the markdown document, so code and manifest cannot silently diverge.
struct ParamMeta
{
    enum class Type { Float, Choice, Bool, Int };

    const char* id;
    const char* displayName;
    Type type;
    float min = 0, max = 1, def = 0;   // Choice/Bool: def = index / 0-1
    float skew = 1.0f;
    const char* unit = "";
    const char* choices = nullptr;      // "A|B|C" for Choice
    int smoothMs = 0;
    bool automatable = true;
};

const std::vector<ParamMeta>& allParams();

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace vfa::params
