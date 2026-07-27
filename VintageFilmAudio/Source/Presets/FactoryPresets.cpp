#include <string_view>
#include "FactoryPresets.h"
#include "../Parameters/ParameterIDs.h"
#include "../Parameters/EraProfiles.h"

// Factory presets are built as Era/Medium/Condition profile applications
// plus preset-specific intent overrides — exactly what a user gets by making
// the same selections, so presets and the profile system cannot drift apart.

namespace vfa::presets
{
using namespace vfa::dsp;
namespace id = vfa::pid;

namespace
{
std::vector<std::pair<const char*, float>> make (Era era, Medium medium, Condition cond,
                                                 std::vector<std::pair<const char*, float>> overrides)
{
    // Context params first so the UI reflects the selection.
    std::vector<std::pair<const char*, float>> v = {
        { id::era, (float) era }, { id::medium, (float) medium }, { id::condition, (float) cond } };
    for (const auto& p : profiles::profileFor (era, medium, cond))
        v.push_back (p);
    for (const auto& p : overrides)
    {
        bool replaced = false;
        for (auto& e : v)
            if (std::string_view (e.first) == p.first) { e.second = p.second; replaced = true; break; }
        if (! replaced) v.push_back (p);
    }
    return v;
}
} // namespace

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "1950s Theater Dialogue",
          make (Era::early1950s, Medium::opticalMono, Condition::releasePrint,
                { { id::dynDialog, 0.35f }, { id::character, 0.4f } }) },
        { "1950s Magnetic Widescreen",
          make (Era::late1950s, Medium::magneticFilm, Condition::master,
                { { id::reproWidth, 0.9f }, { id::fidelity, 0.8f } }) },
        { "Early Television Kinescope",
          make (Era::early1950s, Medium::kinescope, Condition::broadcast,
                { { id::artifacts, 0.45f }, { id::noiseMacro, 0.45f } }) },
        { "1960s Studio Boom",
          make (Era::early1960s, Medium::magneticFilm, Condition::master,
                { { id::dynDialog, 0.3f }, { id::dynComp, 0.35f } }) },
        { "1960s TV Sitcom Mono",
          make (Era::early1960s, Medium::broadcastMono, Condition::broadcast,
                { { id::nsHumFreq, 1.0f }, { id::reproSpeaker, 0.4f } }) },
        { "1960s Dubbed Adventure",
          make (Era::late1960s, Medium::opticalMono, Condition::multiGeneration,
                { { id::dynDialog, 0.45f }, { id::medOpticalDist, 0.5f }, { id::character, 0.6f } }) },
        { "1970s Location Recorder",
          make (Era::early1970s, Medium::fieldTape, Condition::master,
                { { id::nsHiss, 0.4f } }) },
        { "1970s Theatrical Optical",
          make (Era::early1970s, Medium::opticalMono, Condition::releasePrint, {}) },
        { "1977 Optical Stereo",
          make (Era::late1970s, Medium::opticalStereo, Condition::releasePrint,
                { { id::reproWidth, 0.8f } }) },
        { "1980s Broadcast Mono",
          make (Era::early1980s, Medium::broadcastMono, Condition::broadcast,
                { { id::fidelity, 0.75f } }) },
        { "1984 Television Stereo",
          make (Era::late1980s, Medium::broadcastStereo, Condition::broadcast, {}) },
        { "1980s Magnetic Mix",
          make (Era::early1980s, Medium::magneticFilm, Condition::master,
                { { id::fidelity, 0.85f } }) },
        { "Late Analog Cinema",
          make (Era::late1980s, Medium::opticalStereo, Condition::releasePrint,
                { { id::deliveryCurve, 3.0f }, { id::fidelity, 0.85f } }) },
        { "Multi-Generation Workprint",
          make (Era::late1960s, Medium::magneticFilm, Condition::workprint,
                { { id::genVariability, 0.6f } }) },
        { "Worn Archive Print",
          make (Era::late1950s, Medium::opticalMono, Condition::wornArchive,
                { { id::artifacts, 0.55f } }) },
        { "Off-Air Recording",
          make (Era::late1970s, Medium::consumer, Condition::offAir,
                { { id::nsBuzz, 0.3f } }) },
    };
    return presets;
}

} // namespace vfa::presets
