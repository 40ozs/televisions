#include "../Harness/VfaTest.h"
#include <string_view>
#include <map>
#include "Parameters/EraProfiles.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterLayout.h"
#include "Plugin/PluginProcessor.h"

// Era/Medium/Condition profiles (PRD PR-01): every combination must produce
// only known parameter IDs with in-range values, and applying a profile must
// leave parameters editable (it is a plain base-value write, ADR-003).

namespace
{
struct Range { float lo, hi; };

std::map<std::string, Range, std::less<>> manifestRanges()
{
    std::map<std::string, Range, std::less<>> ranges;
    for (const auto& meta : vfa::params::allParams())
    {
        float lo = meta.min, hi = meta.max;
        if (meta.type == vfa::params::ParamMeta::Type::Choice)
        {
            int count = 1;
            for (const char* c = meta.choices; *c; ++c)
                if (*c == '|') ++count;
            lo = 0.0f; hi = float (count - 1);
        }
        else if (meta.type == vfa::params::ParamMeta::Type::Bool)
        {
            lo = 0.0f; hi = 1.0f;
        }
        ranges[meta.id] = { lo, hi };
    }
    return ranges;
}
} // namespace

VFA_TEST (EraProfile_all_combinations_in_range)
{
    const auto ranges = manifestRanges();
    int combos = 0;
    for (int e = 0; e < vfa::dsp::numEras; ++e)
        for (int m = 0; m < vfa::dsp::numMediums; ++m)
            for (int c = 0; c < vfa::dsp::numConditions; ++c)
            {
                ++combos;
                const auto writes = vfa::profiles::profileFor (
                    (vfa::dsp::Era) e, (vfa::dsp::Medium) m, (vfa::dsp::Condition) c);
                CHECK_MSG (! writes.empty(), "empty profile");
                for (const auto& [pid, value] : writes)
                {
                    const auto it = ranges.find (std::string_view (pid));
                    char msg[160];
                    std::snprintf (msg, sizeof (msg), "combo %d/%d/%d param %s value %g",
                                   e, m, c, pid, value);
                    CHECK_MSG (it != ranges.end(), msg);
                    if (it != ranges.end())
                        CHECK_MSG (value >= it->second.lo - 1.0e-5f
                                   && value <= it->second.hi + 1.0e-5f, msg);
                    CHECK_MSG (std::isfinite (value), msg);
                }
                // Profiles never write macros or the era/medium/condition
                // selectors themselves (ADR-003).
                for (const auto& [pid, value] : writes)
                {
                    const std::string_view id (pid);
                    CHECK (id != "character" && id != "fidelity" && id != "artifacts"
                           && id != "noiseMacro" && id != "era" && id != "medium"
                           && id != "condition");
                    juce::ignoreUnused (value);
                }
            }
    CHECK (combos == vfa::dsp::numEras * vfa::dsp::numMediums * vfa::dsp::numConditions);
}

VFA_TEST (EraProfile_apply_is_editable_after)
{
    vfa::VfaProcessor proc;
    proc.applyEraProfile();
    auto* p = proc.parameters().getParameter (vfa::pid::wow);
    REQUIRE (p != nullptr);
    p->setValueNotifyingHost (0.9f);
    CHECK_NEAR (p->getValue(), 0.9f, 1.0e-5);
}

VFA_TEST (EraProfile_condition_degrades)
{
    using namespace vfa::dsp;
    // Damaged condition must carry more generations than Master for the
    // same era/medium.
    auto genOf = [] (Condition c)
    {
        for (const auto& [pid, v] : vfa::profiles::profileFor (Era::early1960s,
                                                               Medium::opticalMono, c))
            if (std::string_view (pid) == "generation") return v;
        return -1.0f;
    };
    CHECK (genOf (Condition::damaged) > genOf (Condition::master));
    CHECK (genOf (Condition::multiGeneration) >= 5.0f);
}
