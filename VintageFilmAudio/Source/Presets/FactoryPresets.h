#pragma once

#include <vector>
#include <utility>

namespace vfa::presets
{

// Factory presets (Docs/PRESET_GUIDE.md). Each preset is a full-intent
// parameter write list applied over defaults; names are trademark-reviewed
// (Docs/NAMING_REVIEW.md — PresetTest cross-checks against the cleared list).
struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;   // (paramID, plain value)
};

const std::vector<FactoryPreset>& factoryPresets();

} // namespace vfa::presets
