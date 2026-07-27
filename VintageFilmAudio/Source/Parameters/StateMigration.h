#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ParameterLayout.h"

// State versioning & migration (ADR-011). v1 is the baseline schema.
// Loading rules: unknown params ignored; missing params keep defaults;
// out-of-range values clamped; malformed input -> caller falls back to the
// default state and sets a non-fatal flag. Future schema changes add
// entries to migrateInPlace() keyed on the incoming version.

namespace vfa::state
{

inline constexpr int kStateVersion = 1;
inline constexpr auto kVersionProperty = "stateVersion";

enum class LoadResult { ok, migrated, corrupted };

// Clamp any parameter child values to their manifest ranges.
inline void clampToManifest (juce::ValueTree& state)
{
    for (const auto& meta : params::allParams())
    {
        auto child = state.getChildWithProperty ("id", juce::String (meta.id));
        if (! child.isValid()) continue;
        if (! child.hasProperty ("value")) continue;
        const float v = (float) child.getProperty ("value");
        float lo = meta.min, hi = meta.max;
        if (meta.type == params::ParamMeta::Type::Choice)
        {
            juce::StringArray options;
            options.addTokens (juce::String (meta.choices), "|", "");
            lo = 0.0f; hi = (float) (options.size() - 1);
        }
        else if (meta.type == params::ParamMeta::Type::Bool)
        {
            lo = 0.0f; hi = 1.0f;
        }
        if (v < lo || v > hi || ! std::isfinite (v))
            child.setProperty ("value", juce::jlimit (lo, hi, std::isfinite (v) ? v : lo), nullptr);
    }
}

inline LoadResult migrateInPlace (juce::ValueTree& state)
{
    if (! state.isValid() || ! state.hasType ("PARAMS"))
        return LoadResult::corrupted;

    const int version = state.getProperty (kVersionProperty, 1);

    // Future migrations: if (version < 2) { ...transform...; }
    clampToManifest (state);
    state.setProperty (kVersionProperty, kStateVersion, nullptr);

    return version == kStateVersion ? LoadResult::ok : LoadResult::migrated;
}

} // namespace vfa::state
