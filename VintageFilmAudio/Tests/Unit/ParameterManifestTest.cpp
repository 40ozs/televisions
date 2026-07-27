#include "../Harness/VfaTest.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters/ParameterLayout.h"
#include "Plugin/PluginProcessor.h"

// Cross-checks Docs/PARAMETER_MANIFEST.md against the in-code table and the
// actual APVTS layout, so code and manifest cannot silently diverge (SAS §4).

namespace
{
struct ManifestRow
{
    std::string id, display, type, range, def, skew, unit, smooth, autoFlag;
};

std::vector<ManifestRow> parseManifest()
{
    std::vector<ManifestRow> rows;
    std::ifstream in (std::filesystem::path (VFA_REPO_ROOT) / "Docs" / "PARAMETER_MANIFEST.md");
    REQUIRE (in.good());

    std::string line;
    bool inTable = false;
    while (std::getline (in, line))
    {
        if (line.rfind ("| ID |", 0) == 0) { inTable = true; std::getline (in, line); continue; }
        if (! inTable) continue;
        if (line.empty() || line[0] != '|') break;

        // Split on '|' (cells may contain escaped \| for choice lists).
        std::vector<std::string> cells;
        std::string cell;
        for (size_t i = 1; i < line.size(); ++i)
        {
            if (line[i] == '\\' && i + 1 < line.size() && line[i + 1] == '|')
            {
                cell += '|'; ++i; continue;
            }
            if (line[i] == '|') { cells.push_back (cell); cell.clear(); continue; }
            cell += line[i];
        }
        auto trim = [] (std::string s)
        {
            const auto a = s.find_first_not_of (" \t");
            const auto b = s.find_last_not_of (" \t");
            return a == std::string::npos ? std::string() : s.substr (a, b - a + 1);
        };
        if (cells.size() < 10) continue;
        rows.push_back ({ trim (cells[0]), trim (cells[1]), trim (cells[2]), trim (cells[3]),
                          trim (cells[4]), trim (cells[5]), trim (cells[6]), trim (cells[7]),
                          trim (cells[8]) });
    }
    return rows;
}
} // namespace

VFA_TEST (Manifest_matches_code_table)
{
    const auto rows = parseManifest();
    const auto& code = vfa::params::allParams();

    char msg[256];
    std::snprintf (msg, sizeof (msg), "manifest rows %d vs code %d",
                   (int) rows.size(), (int) code.size());
    CHECK_MSG (rows.size() == code.size(), msg);

    for (size_t i = 0; i < std::min (rows.size(), code.size()); ++i)
    {
        const auto& r = rows[i];
        const auto& c = code[i];
        using T = vfa::params::ParamMeta::Type;

        std::snprintf (msg, sizeof (msg), "row %d id '%s' vs '%s'", (int) i, r.id.c_str(), c.id);
        CHECK_MSG (r.id == c.id, msg);
        CHECK_MSG (r.display == c.displayName, (r.id + ": display '" + r.display + "' vs '" + c.displayName + "'").c_str());

        const char* expectType = c.type == T::Float ? "float"
                                : c.type == T::Choice ? "choice"
                                : c.type == T::Bool ? "bool" : "int";
        CHECK_MSG (r.type == expectType, (r.id + ": type").c_str());

        if (c.type == T::Float || c.type == T::Int)
        {
            const auto dots = r.range.find ("..");
            REQUIRE (dots != std::string::npos);
            const float lo = std::stof (r.range.substr (0, dots));
            const float hi = std::stof (r.range.substr (dots + 2));
            CHECK_NEAR (lo, c.min, 1.0e-4);
            CHECK_NEAR (hi, c.max, 1.0e-4);
            CHECK_NEAR (std::stof (r.def), c.def, 1.0e-4);
            if (r.skew != "-")
                CHECK_NEAR (std::stof (r.skew), c.skew, 1.0e-4);
        }
        else if (c.type == T::Choice)
        {
            CHECK_MSG (r.range == c.choices, (r.id + ": choices '" + r.range + "' vs '" + c.choices + "'").c_str());
            // Default is the choice label.
            juce::StringArray options;
            options.addTokens (juce::String (c.choices), "|", "");
            const int defIdx = (int) c.def;
            REQUIRE (defIdx >= 0 && defIdx < options.size());
            CHECK_MSG (r.def == options[defIdx].toStdString(), (r.id + ": default choice").c_str());
        }
        else // Bool
        {
            CHECK_MSG (r.def == (c.def >= 0.5f ? "on" : "off"), (r.id + ": bool default").c_str());
        }

        const std::string unit = r.unit == "-" ? "" : r.unit;
        CHECK_MSG (unit == c.unit, (r.id + ": unit").c_str());
        CHECK_MSG (std::stoi (r.smooth) == c.smoothMs, (r.id + ": smooth ms").c_str());
        CHECK_MSG ((r.autoFlag == "y") == c.automatable, (r.id + ": automatable").c_str());
    }
}

VFA_TEST (Apvts_layout_contains_all_params)
{
    vfa::VfaProcessor proc;
    for (const auto& meta : vfa::params::allParams())
    {
        auto* p = proc.parameters().getParameter (meta.id);
        CHECK_MSG (p != nullptr, meta.id);
        if (p == nullptr) continue;
        if (meta.type == vfa::params::ParamMeta::Type::Float)
        {
            auto* fp = dynamic_cast<juce::AudioParameterFloat*> (p);
            CHECK_MSG (fp != nullptr, meta.id);
            if (fp != nullptr)
            {
                CHECK_NEAR (fp->range.start, meta.min, 1.0e-4);
                CHECK_NEAR (fp->range.end, meta.max, 1.0e-4);
                CHECK_NEAR (fp->get(), meta.def, 1.0e-3);
            }
        }
        CHECK_MSG (p->isAutomatable() == meta.automatable, meta.id);
    }
}
