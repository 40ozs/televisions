#include "../Harness/VfaTest.h"
#include "Plugin/PluginProcessor.h"
#include "DSP/Core/Rng.h"

// NaN/Inf and stability stress (PRD PR-12, TEST_PLAN §Realtime).

namespace
{
void fillSignal (juce::AudioBuffer<float>& buf, int kind, vfa::dsp::Rng& rng, int startSample)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        auto* d = buf.getWritePointer (ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
        {
            const int t = startSample + i;
            switch (kind)
            {
                case 0: d[i] = 0.126f * (float) std::sin (2.0 * M_PI * 997.0 * t / 48000.0); break;
                case 1: d[i] = ((t / 120) & 1) ? 0.98f : -0.98f; break;          // square
                case 2: d[i] = rng.nextBipolar() * 0.5f; break;                  // noise
                case 3: d[i] = 1.0e-30f; break;                                  // denormal region
                case 4: d[i] = 0.0f; break;                                      // silence
            }
        }
    }
}

// Extreme trim settings can legitimately produce hot output when the safety
// limiter is switched off (+24 dB trim on a full-scale square ~ 16x); the
// stress bound only guards against NaN/Inf and runaway feedback, not against
// the user's own gain staging.
bool bufferClean (const juce::AudioBuffer<float>& buf)
{
    for (int ch = 0; ch < buf.getNumChannels(); ++ch)
    {
        const auto* d = buf.getReadPointer (ch);
        for (int i = 0; i < buf.getNumSamples(); ++i)
            if (! std::isfinite (d[i]) || std::abs (d[i]) > 64.0f)
                return false;
    }
    return true;
}

void setAllParams (vfa::VfaProcessor& proc, float normValue)
{
    for (const auto& meta : vfa::params::allParams())
        if (auto* p = proc.parameters().getParameter (meta.id))
            p->setValueNotifyingHost (normValue);
}
} // namespace

VFA_TEST (Stress_extreme_parameter_corners)
{
    for (float corner : { 0.0f, 1.0f })
    {
        vfa::VfaProcessor proc;
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        setAllParams (proc, corner);

        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        vfa::dsp::Rng rng (1);
        for (int kind = 0; kind < 5; ++kind)
            for (int b = 0; b < 30; ++b)
            {
                fillSignal (buf, kind, rng, b * 512);
                proc.processBlock (buf, midi);
                if (! bufferClean (buf))
                {
                    char msg[96];
                    std::snprintf (msg, sizeof (msg), "corner %.0f kind %d block %d", corner, kind, b);
                    CHECK_MSG (false, msg);
                    return;
                }
                // All-max includes safetyLimiter ON (final stage): even with
                // +24 dB trims the output must stay at the ceiling.
                if (corner == 1.0f && b > 10)
                {
                    float peak = 0.0f;
                    for (int i = 0; i < 512; ++i)
                        peak = std::max (peak, std::abs (buf.getReadPointer (0)[i]));
                    CHECK_MSG (peak <= 1.02f, "safety limiter did not hold ceiling at all-max");
                }
            }
        CHECK (true);
    }
}

VFA_TEST (Stress_random_parameter_states)
{
    vfa::VfaProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    vfa::dsp::Rng rng (77), sig (3);
    for (int state = 0; state < 200; ++state)
    {
        for (const auto& meta : vfa::params::allParams())
            if (auto* p = proc.parameters().getParameter (meta.id))
                p->setValueNotifyingHost (rng.next01());
        for (int b = 0; b < 4; ++b)
        {
            fillSignal (buf, (int) (rng.nextU64() % 5), sig, (state * 4 + b) * 512);
            proc.processBlock (buf, midi);
            CHECK_MSG (bufferClean (buf), "random state produced NaN/Inf/huge output");
        }
    }
}

VFA_TEST (Stress_block_size_changes_and_zero_length)
{
    vfa::VfaProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 2048);
    proc.prepareToPlay (48000.0, 2048);

    juce::MidiBuffer midi;
    vfa::dsp::Rng sig (9);
    for (int n : { 0, 1, 3, 16, 17, 100, 512, 1024, 2048, 5, 2048, 0, 64 })
    {
        juce::AudioBuffer<float> buf (2, std::max (n, 1));
        buf.setSize (2, n, false, false, true);
        if (n > 0) fillSignal (buf, 2, sig, 0);
        proc.processBlock (buf, midi);
        if (n > 0)
            CHECK_MSG (bufferClean (buf), "block-size change produced bad output");
    }
    CHECK (true);
}

VFA_TEST (Stress_sample_rate_changes)
{
    vfa::VfaProcessor proc;
    juce::MidiBuffer midi;
    vfa::dsp::Rng sig (11);
    for (double sr : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        proc.setPlayConfigDetails (2, 2, sr, 512);
        proc.prepareToPlay (sr, 512);
        juce::AudioBuffer<float> buf (2, 512);
        for (int b = 0; b < 20; ++b)
        {
            fillSignal (buf, b % 5, sig, b * 512);
            proc.processBlock (buf, midi);
            char msg[64];
            std::snprintf (msg, sizeof (msg), "sr %.0f block %d", sr, b);
            CHECK_MSG (bufferClean (buf), msg);
        }
    }
}

VFA_TEST (Stress_rapid_preset_changes_during_playback)
{
    vfa::VfaProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 256);
    proc.prepareToPlay (48000.0, 256);

    juce::AudioBuffer<float> buf (2, 256);
    juce::MidiBuffer midi;
    vfa::dsp::Rng sig (13);
    for (int b = 0; b < 200; ++b)
    {
        if (b % 3 == 0)
            proc.setCurrentProgram (b / 3 % proc.getNumPrograms());
        fillSignal (buf, 0, sig, b * 256);
        proc.processBlock (buf, midi);
        CHECK_MSG (bufferClean (buf), "preset storm produced bad output");
    }
}

VFA_TEST (Stress_state_reload_during_playback)
{
    vfa::VfaProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 256);
    proc.prepareToPlay (48000.0, 256);

    juce::MemoryBlock saved;
    proc.getStateInformation (saved);

    juce::AudioBuffer<float> buf (2, 256);
    juce::MidiBuffer midi;
    vfa::dsp::Rng sig (17);
    for (int b = 0; b < 100; ++b)
    {
        if (b % 10 == 5)
            proc.setStateInformation (saved.getData(), (int) saved.getSize());
        fillSignal (buf, 2, sig, b * 256);
        proc.processBlock (buf, midi);
        CHECK_MSG (bufferClean (buf), "state reload during playback produced bad output");
    }
}
