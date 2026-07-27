#include "EraProfiles.h"
#include "ParameterIDs.h"

// Era/Medium/Condition -> coherent base-parameter state (PRD PR-01).
// Values are historically-informed engineering choices (HIST-APPROX where the
// spec says so); every write stays inside the manifest ranges (EraProfileTest
// asserts this exhaustively for all 8 x 9 x 8 combinations).

namespace vfa::profiles
{
using namespace vfa::dsp;
namespace id = vfa::pid;

std::vector<ParamWrite> profileFor (Era era, Medium medium, Condition condition)
{
    std::vector<ParamWrite> w;
    const int eraIdx = (int) era;
    const float eraLate = eraIdx / 7.0f;   // 0 = early 50s .. 1 = late 80s

    // --- medium-driven core settings
    switch (medium)
    {
        case Medium::opticalMono:
            w = { { id::deliveryCurve, eraIdx >= (int) Era::early1970s ? 2.0f : 1.0f }, // X-Curve era vs Academy
                  { id::medOpticalDist, 0.45f - 0.15f * eraLate },
                  { id::medImageSpread, 0.45f - 0.15f * eraLate },
                  { id::medOpticalMode, eraIdx <= (int) Era::late1950s ? 1.0f : 0.0f },
                  { id::nsCell, 0.3f - 0.1f * eraLate }, { id::nsHiss, 0.15f },
                  { id::nsProjector, 0.15f }, { id::nsCrackle, 0.2f }, { id::nsDirt, 0.15f },
                  { id::reproMono, 2.0f }, { id::dynComp, 0.35f }, { id::dynLimit, 0.35f },
                  { id::wow, 0.15f }, { id::flutter, 0.2f }, { id::transportQuality, 1.0f } };
            break;
        case Medium::opticalStereo:
            w = { { id::deliveryCurve, 2.0f },
                  { id::medOpticalDist, 0.25f }, { id::medImageSpread, 0.25f },
                  { id::medOpticalMode, 0.0f },
                  { id::nsCell, 0.15f }, { id::nsHiss, 0.12f }, { id::nsProjector, 0.08f },
                  { id::nsCrackle, 0.1f }, { id::nsDirt, 0.08f },
                  { id::reproMono, 0.0f }, { id::reproWidth, 0.85f }, { id::medCrosstalk, 0.35f },
                  { id::dynComp, 0.3f }, { id::dynLimit, 0.3f },
                  { id::wow, 0.1f }, { id::flutter, 0.15f }, { id::transportQuality, 1.0f } };
            break;
        case Medium::magneticFilm:
            w = { { id::deliveryCurve, eraIdx >= (int) Era::early1970s ? 2.0f : 1.0f },
                  { id::medMagSat, 0.35f }, { id::medTapeSpeed, 3.0f }, { id::medHeadBump, 0.5f },
                  { id::nsHiss, 0.25f - 0.1f * eraLate }, { id::nsCell, 0.0f },
                  { id::nsPrintThrough, 0.15f }, { id::nsCrackle, 0.05f }, { id::nsDirt, 0.03f },
                  { id::reproMono, eraIdx <= (int) Era::early1950s ? 2.0f : 0.0f },
                  { id::dynComp, 0.3f }, { id::dynLimit, 0.25f },
                  { id::wow, 0.08f }, { id::flutter, 0.12f }, { id::transportQuality, 1.0f } };
            break;
        case Medium::fieldTape:
            w = { { id::deliveryCurve, 0.0f },
                  { id::medMagSat, 0.4f }, { id::medTapeSpeed, 1.0f }, { id::medHeadBump, 0.6f },
                  { id::nsHiss, 0.35f }, { id::nsPrintThrough, 0.2f },
                  { id::nsCrackle, 0.05f }, { id::nsDirt, 0.05f },
                  { id::reproMono, 2.0f }, { id::dynAgc, 0.3f }, { id::dynComp, 0.35f },
                  { id::wow, 0.25f }, { id::flutter, 0.3f }, { id::drift, 0.3f },
                  { id::transportQuality, 2.0f } };
            break;
        case Medium::kinescope:
            w = { { id::deliveryCurve, 5.0f },
                  { id::medOpticalDist, 0.3f }, { id::medImageSpread, 0.3f }, { id::medOpticalMode, 1.0f },
                  { id::nsCell, 0.25f }, { id::nsHiss, 0.2f }, { id::nsHum, 0.3f },
                  { id::nsBuzz, 0.25f }, { id::nsCrackle, 0.2f }, { id::nsDirt, 0.15f },
                  { id::reproMono, 2.0f }, { id::dynAgc, 0.35f }, { id::dynComp, 0.4f },
                  { id::dynDialog, 0.35f },
                  { id::wow, 0.2f }, { id::flutter, 0.25f }, { id::transportQuality, 2.0f } };
            break;
        case Medium::broadcastMono:
            w = { { id::deliveryCurve, eraIdx >= (int) Era::early1970s ? 6.0f : 4.0f },
                  { id::nsBroadcast, 0.3f }, { id::nsHum, 0.25f }, { id::nsBuzz, 0.2f },
                  { id::nsHiss, 0.1f },
                  { id::reproMono, 2.0f }, { id::reproSpeaker, 0.35f },
                  { id::dynAgc, 0.45f }, { id::dynComp, 0.4f }, { id::dynLimit, 0.45f },
                  { id::dynDialog, 0.4f },
                  { id::wow, 0.05f }, { id::flutter, 0.08f }, { id::transportQuality, 1.0f } };
            break;
        case Medium::broadcastStereo:
            w = { { id::deliveryCurve, 7.0f },
                  { id::nsBroadcast, 0.25f }, { id::nsHum, 0.12f }, { id::nsBuzz, 0.1f },
                  { id::nsHiss, 0.08f },
                  { id::reproMono, 0.0f }, { id::reproWidth, 0.9f }, { id::reproSpeaker, 0.25f },
                  { id::dynAgc, 0.4f }, { id::dynComp, 0.4f }, { id::dynLimit, 0.45f },
                  { id::dynDialog, 0.35f },
                  { id::wow, 0.04f }, { id::flutter, 0.06f }, { id::transportQuality, 1.0f } };
            break;
        case Medium::consumer:
            w = { { id::deliveryCurve, 7.0f },
                  { id::medMagSat, 0.5f }, { id::medTapeSpeed, 0.0f }, { id::medHeadBump, 0.55f },
                  { id::nsHiss, 0.45f }, { id::nsHum, 0.25f }, { id::nsBuzz, 0.15f },
                  { id::nsDropout, 0.25f },
                  { id::reproMono, 1.0f }, { id::reproSpeaker, 0.3f },
                  { id::dynAgc, 0.5f }, { id::dynComp, 0.3f },
                  { id::wow, 0.35f }, { id::flutter, 0.35f }, { id::drift, 0.4f },
                  { id::transportQuality, 3.0f } };
            break;
        case Medium::clean:
            w = { { id::deliveryCurve, 0.0f }, { id::nsHiss, 0.0f }, { id::nsCell, 0.0f },
                  { id::nsHum, 0.0f }, { id::nsCrackle, 0.0f }, { id::nsDirt, 0.0f },
                  { id::wow, 0.0f }, { id::flutter, 0.0f }, { id::drift, 0.0f },
                  { id::dynComp, 0.1f }, { id::reproMono, 0.0f } };
            break;
    }

    // --- era-driven adjustments (bandwidth and noise improve with time)
    w.push_back ({ id::delHfRoll, 6000.0f + 12000.0f * eraLate * 0.7f
                                   + (medium == Medium::magneticFilm ? 3000.0f : 0.0f) });
    w.push_back ({ id::delLfRoll, 80.0f - 45.0f * eraLate });

    // --- condition-driven degradation on top
    auto scaleUp = [&w] (const char* pid, float mult, float cap = 1.0f)
    {
        for (auto& e : w)
            if (e.first == pid) { e.second = std::min (e.second * mult, cap); return; }
    };
    switch (condition)
    {
        case Condition::master: w.push_back ({ id::generation, 0.0f }); break;
        case Condition::releasePrint: w.push_back ({ id::generation, 2.0f }); break;
        case Condition::broadcast: w.push_back ({ id::generation, 2.0f }); break;
        case Condition::offAir:
            w.push_back ({ id::generation, 3.0f });
            w.push_back ({ id::nsDropout, 0.3f });
            scaleUp (id::nsHum, 1.6f);
            scaleUp (id::nsBuzz, 1.6f);
            break;
        case Condition::workprint:
            w.push_back ({ id::generation, 4.0f });
            w.push_back ({ id::genVariability, 0.5f });
            scaleUp (id::nsCrackle, 1.8f);
            scaleUp (id::nsDirt, 1.8f);
            break;
        case Condition::wornArchive:
            w.push_back ({ id::generation, 3.0f });
            w.push_back ({ id::nsCrackle, 0.55f });
            w.push_back ({ id::nsDirt, 0.5f });
            w.push_back ({ id::nsDropout, 0.25f });
            scaleUp (id::wow, 1.8f);
            scaleUp (id::flutter, 1.6f);
            break;
        case Condition::multiGeneration:
            w.push_back ({ id::generation, 6.0f });
            w.push_back ({ id::genVariability, 0.5f });
            break;
        case Condition::damaged:
            w.push_back ({ id::generation, 5.0f });
            w.push_back ({ id::nsCrackle, 0.7f });
            w.push_back ({ id::nsDirt, 0.65f });
            w.push_back ({ id::nsDropout, 0.5f });
            w.push_back ({ id::transportQuality, 4.0f });
            scaleUp (id::wow, 2.5f);
            scaleUp (id::flutter, 2.0f);
            break;
    }

    return w;
}

} // namespace vfa::profiles
