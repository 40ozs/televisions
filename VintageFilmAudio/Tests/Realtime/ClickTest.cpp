#include "../Harness/VfaTest.h"
#include <string_view>
#include <set>
#include "Plugin/PluginProcessor.h"
#include "DSP/Core/Rng.h"

// Automation click safety (TEST_PLAN §Realtime): ramping/toggling controls
// during a steady sine must not create discontinuities beyond a bounded
// sample-to-sample step. Event-type artifact controls are excluded — their
// output is impulsive BY DESIGN (crackle/dirt/dropout/projector), as is the
// Audition monitoring switch.

namespace
{
constexpr double kStepLimit = 0.08;   // 220 Hz sine @ -18 dBFS natural step ~0.0036

const std::set<std::string, std::less<>> excluded = {
    "nsCrackle", "nsDirt", "nsDropout", "nsProjector", "audition", "seed",
};

struct ClickProbe
{
    vfa::VfaProcessor proc;
    juce::AudioBuffer<float> buf { 2, 256 };
    juce::MidiBuffer midi;
    long sampleIndex = 0;
    float lastSample = 0.0f;
    double maxStep = 0.0;
    bool sawBad = false;

    ClickProbe()
    {
        proc.setPlayConfigDetails (2, 2, 48000.0, 256);
        proc.prepareToPlay (48000.0, 256);
        // Quiet baseline: impulsive artifact sources at zero.
        for (const char* pid : { "nsCrackle", "nsDirt", "nsDropout", "nsProjector" })
            if (auto* p = proc.parameters().getParameter (pid))
                p->setValueNotifyingHost (0.0f);
    }

    void run (int blocks, bool measure = true)
    {
        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buf.getWritePointer (ch);
                for (int i = 0; i < 256; ++i)
                    d[i] = 0.126f * (float) std::sin (2.0 * M_PI * 220.0 * (sampleIndex + i) / 48000.0);
            }
            proc.processBlock (buf, midi);
            sampleIndex += 256;
            const auto* out = buf.getReadPointer (0);
            for (int i = 0; i < 256; ++i)
            {
                if (! std::isfinite (out[i])) { sawBad = true; return; }
                if (measure)
                    maxStep = std::max (maxStep, (double) std::abs (out[i] - lastSample));
                lastSample = out[i];
            }
        }
    }
};
} // namespace

VFA_TEST (Click_float_parameter_ramps)
{
    ClickProbe probe;
    probe.run (60, false);   // settle
    probe.maxStep = 0;

    for (const auto& meta : vfa::params::allParams())
    {
        if (meta.type != vfa::params::ParamMeta::Type::Float) continue;
        if (excluded.count (std::string_view (meta.id))) continue;
        auto* p = probe.proc.parameters().getParameter (meta.id);
        REQUIRE (p != nullptr);
        // Ramp min -> max -> default over ~0.4 s in coarse host-style steps.
        for (int s = 0; s <= 12; ++s)
        {
            p->setValueNotifyingHost (s <= 6 ? s / 6.0f : (12 - s) / 6.0f);
            probe.run (3);
        }
        p->setValueNotifyingHost (p->getDefaultValue());
        probe.run (8, false);
        char msg[128];
        std::snprintf (msg, sizeof (msg), "%s: max step %.4f", meta.id, probe.maxStep);
        CHECK_MSG (! probe.sawBad && probe.maxStep < kStepLimit, msg);
        probe.maxStep = 0;
    }
}

VFA_TEST (Click_toggles_and_choices)
{
    ClickProbe probe;
    probe.run (60, false);
    probe.maxStep = 0;

    auto exercise = [&probe] (const char* pid, std::initializer_list<float> normValues)
    {
        auto* p = probe.proc.parameters().getParameter (pid);
        REQUIRE (p != nullptr);
        for (float v : normValues)
        {
            p->setValueNotifyingHost (v);
            probe.run (12);   // 64 ms: transition ramps must complete cleanly
        }
        p->setValueNotifyingHost (p->getDefaultValue());
        probe.run (12, false);
        char msg[128];
        std::snprintf (msg, sizeof (msg), "%s: max step %.4f", pid, probe.maxStep);
        CHECK_MSG (! probe.sawBad && probe.maxStep < kStepLimit, msg);
        probe.maxStep = 0;
    };

    // Module enables (ModuleSlot 10 ms crossfades).
    for (const char* pid : { "deliveryOn", "mediumOn", "transportOn", "genOn",
                             "noiseOn", "dynOn", "reproOn" })
        exercise (pid, { 0.0f, 1.0f, 0.0f, 1.0f });

    // Medium and quality switches (Engine 5 ms wet-path transition, DEV-004).
    exercise ("medium", { 0.0f, 0.3f, 0.6f, 1.0f, 0.4f });
    exercise ("quality", { 0.0f, 0.5f, 1.0f, 0.5f });

    // Remaining discrete controls.
    exercise ("medOpticalMode", { 0.0f, 1.0f });
    exercise ("medTapeSpeed", { 0.0f, 1.0f, 0.33f });
    exercise ("nsHumFreq", { 0.0f, 1.0f });
    exercise ("reproMono", { 0.0f, 0.5f, 1.0f });
    exercise ("monoLaw", { 0.0f, 0.5f, 1.0f });
    exercise ("stereoLink", { 0.0f, 1.0f });
    exercise ("genInteger", { 0.0f, 1.0f });
    exercise ("safetyLimiter", { 0.0f, 1.0f });
    exercise ("autoGain", { 0.0f, 1.0f });
}

VFA_TEST (Click_host_bypass_transition)
{
    // Host bypass is host-side, but our own full-wet <-> mix transitions and
    // enable ramps stand in for it; additionally verify mix 100 -> 0 -> 100.
    ClickProbe probe;
    probe.run (60, false);
    probe.maxStep = 0;
    auto* mix = probe.proc.parameters().getParameter ("mix");
    REQUIRE (mix != nullptr);
    for (float v : { 0.0f, 1.0f, 0.0f, 1.0f })
    {
        mix->setValueNotifyingHost (v);
        probe.run (10);
    }
    char msg[96];
    std::snprintf (msg, sizeof (msg), "mix bypass: max step %.4f", probe.maxStep);
    CHECK_MSG (! probe.sawBad && probe.maxStep < kStepLimit, msg);
}
