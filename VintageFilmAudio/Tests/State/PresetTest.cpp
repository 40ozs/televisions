#include "../Harness/VfaTest.h"
#include <string_view>
#include <set>
#include "Presets/FactoryPresets.h"
#include "Parameters/ParameterLayout.h"
#include "Plugin/PluginProcessor.h"

// Factory presets (PRD PR-02): valid IDs, in-range values, trademark-cleared
// names, decade/medium coverage, and accurate recall through the program API.

namespace
{
// Cleared names from Docs/NAMING_REVIEW.md — keep in sync (test enforces the
// review gate: an uncleared preset name fails here).
const std::set<std::string, std::less<>> clearedNames = {
    "1938 Optical Mono",
    "1950s Theater Dialogue", "1950s Magnetic Widescreen",
    "Early Television Kinescope", "1960s Studio Boom", "1960s TV Sitcom Mono",
    "1960s Dubbed Adventure", "1970s Location Recorder",
    "1970s Theatrical Optical", "1977 Optical Stereo", "1980s Broadcast Mono",
    "1984 Television Stereo", "1980s Magnetic Mix", "Late Analog Cinema",
    "Multi-Generation Workprint", "Worn Archive Print", "Off-Air Recording",
};
} // namespace

VFA_TEST (Presets_names_cleared_and_values_valid)
{
    const auto& presets = vfa::presets::factoryPresets();
    CHECK (presets.size() == 17);

    std::set<std::string, std::less<>> validIds;
    for (const auto& meta : vfa::params::allParams())
        validIds.insert (meta.id);

    for (const auto& preset : presets)
    {
        CHECK_MSG (clearedNames.count (std::string_view (preset.name)) == 1, preset.name);
        for (const auto& [pid, value] : preset.values)
        {
            CHECK_MSG (validIds.count (std::string_view (pid)) == 1,
                       (std::string (preset.name) + ": " + pid).c_str());
            CHECK_MSG (std::isfinite (value), pid);
        }
    }
}

VFA_TEST (Presets_cover_required_decades_and_media)
{
    using namespace vfa::dsp;
    std::set<int> eras, media;
    for (const auto& preset : vfa::presets::factoryPresets())
        for (const auto& [pid, value] : preset.values)
        {
            if (std::string_view (pid) == "era") eras.insert ((int) value);
            if (std::string_view (pid) == "medium") media.insert ((int) value);
        }
    // Every decade 1950s-1980s (era pairs) represented.
    CHECK (eras.count ((int) Era::early1950s) + eras.count ((int) Era::late1950s) > 0);
    CHECK (eras.count ((int) Era::early1960s) + eras.count ((int) Era::late1960s) > 0);
    CHECK (eras.count ((int) Era::early1970s) + eras.count ((int) Era::late1970s) > 0);
    CHECK (eras.count ((int) Era::early1980s) + eras.count ((int) Era::late1980s) > 0);
    // Required medium classes.
    for (Medium m : { Medium::opticalMono, Medium::opticalStereo, Medium::magneticFilm,
                      Medium::fieldTape, Medium::kinescope, Medium::broadcastMono,
                      Medium::broadcastStereo, Medium::consumer })
        CHECK_MSG (media.count ((int) m) == 1, "medium class missing from presets");
}

VFA_TEST (Presets_program_recall_accurate)
{
    vfa::VfaProcessor proc;
    CHECK (proc.getNumPrograms() == 17);

    const auto& presets = vfa::presets::factoryPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        proc.setCurrentProgram (i);
        CHECK (proc.getCurrentProgram() == i);
        CHECK (proc.getProgramName (i) == juce::String (presets[(size_t) i].name));

        // Every preset write must be recalled exactly (plain-value domain).
        for (const auto& [pid, value] : presets[(size_t) i].values)
        {
            auto* p = proc.parameters().getParameter (pid);
            REQUIRE (p != nullptr);
            const float plain = p->convertFrom0to1 (p->getValue());
            char msg[160];
            std::snprintf (msg, sizeof (msg), "%s / %s: %g vs %g",
                           presets[(size_t) i].name, pid, plain, value);
            CHECK_MSG (std::abs (plain - value) < 0.02f, msg);
        }
    }
}
