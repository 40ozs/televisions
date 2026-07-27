#include "../Harness/VfaTest.h"
#include "Plugin/PluginProcessor.h"
#include "Parameters/ParameterIDs.h"

// Gain staging (PRD PR-14, DSP_SPEC §11): trims, mix law, safety ceiling.
// Uses a "clean lab" configuration: medium Clean, all coloration modules off,
// so level arithmetic is isolated from the historical processing.

namespace
{
void setPlain (vfa::VfaProcessor& proc, const char* pid, float plain)
{
    auto* p = proc.parameters().getParameter (pid);
    REQUIRE (p != nullptr);
    p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

void makeCleanLab (vfa::VfaProcessor& proc)
{
    namespace id = vfa::pid;
    setPlain (proc, id::medium, 0);          // Clean
    for (const char* pid : { id::deliveryOn, id::mediumOn, id::genOn,
                             id::noiseOn, id::dynOn, id::reproOn })
        setPlain (proc, pid, 0);
    setPlain (proc, id::transportOn, 0);     // depth 0 (fixed latency remains)
    setPlain (proc, id::safetyLimiter, 0);
    setPlain (proc, id::autoGain, 0);
    setPlain (proc, id::deliveryCurve, 0);   // Neutral
}

// Renders a sine and returns steady-state output RMS dB (last 50 %).
double renderRmsDb (vfa::VfaProcessor& proc, float ampDb, double seconds = 1.0)
{
    const int n = (int) (48000 * seconds);
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buf (2, n);
    const float amp = std::pow (10.0f, ampDb / 20.0f) * std::sqrt (2.0f); // RMS -> peak
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            d[i] = amp * (float) std::sin (2.0 * M_PI * 1000.0 * i / 48000.0);
    }
    juce::MidiBuffer midi;
    for (int pos = 0; pos < n; pos += 512)
    {
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, pos,
                                       std::min (512, n - pos));
        proc.processBlock (view, midi);
    }
    return vfatest::rmsDb (buf.getReadPointer (0) + n / 2, n / 2);
}
} // namespace

VFA_TEST (Gain_clean_lab_is_unity)
{
    vfa::VfaProcessor proc;
    makeCleanLab (proc);
    const double out = renderRmsDb (proc, -18.0f);
    CHECK_NEAR (out, -18.0, 0.3);
}

VFA_TEST (Gain_trims_sum_correctly)
{
    vfa::VfaProcessor proc;
    makeCleanLab (proc);
    setPlain (proc, vfa::pid::inTrim, 6.0f);
    setPlain (proc, vfa::pid::outTrim, -6.0f);
    CHECK_NEAR (renderRmsDb (proc, -18.0f), -18.0, 0.4);

    setPlain (proc, vfa::pid::inTrim, 0.0f);
    setPlain (proc, vfa::pid::outTrim, 6.0f);
    CHECK_NEAR (renderRmsDb (proc, -18.0f), -12.0, 0.4);
}

VFA_TEST (Gain_mix_zero_is_dry_level)
{
    vfa::VfaProcessor proc;
    makeCleanLab (proc);
    setPlain (proc, vfa::pid::mix, 0.0f);
    setPlain (proc, vfa::pid::inTrim, 12.0f);   // must NOT affect dry path level
    CHECK_NEAR (renderRmsDb (proc, -18.0f), -18.0, 0.4);
}

VFA_TEST (Gain_safety_limiter_holds_ceiling)
{
    vfa::VfaProcessor proc;
    makeCleanLab (proc);
    setPlain (proc, vfa::pid::safetyLimiter, 1);
    setPlain (proc, vfa::pid::inTrim, 12.0f);   // drive a hot signal
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buf (2, 48000);
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        for (int i = 0; i < 48000; ++i)
            d[i] = 0.9f * (float) std::sin (2.0 * M_PI * 200.0 * i / 48000.0);
    }
    juce::MidiBuffer midi;
    for (int pos = 0; pos < 48000; pos += 512)
    {
        juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, pos,
                                       std::min (512, 48000 - pos));
        proc.processBlock (view, midi);
    }
    float peak = 0.0f;
    for (int i = 24000; i < 48000; ++i)
        peak = std::max (peak, std::abs (buf.getReadPointer (0)[i]));
    CHECK_MSG (peak <= 1.0f, "safety limiter exceeded 0 dBFS");
    CHECK (! vfatest::hasNanOrInf (buf.getReadPointer (0), 48000));
}
