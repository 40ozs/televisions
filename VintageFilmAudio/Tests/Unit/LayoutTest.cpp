#include "../Harness/VfaTest.h"
#include "Plugin/PluginProcessor.h"

// Channel layouts (PRD PR-15, ADR-013): mono and stereo instantiate and
// process cleanly; mismatched layouts are refused.

VFA_TEST (Layout_stereo_processes_clean)
{
    vfa::VfaProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 256);
    proc.prepareToPlay (48000.0, 256);
    juce::AudioBuffer<float> buf (2, 256);
    juce::MidiBuffer midi;
    for (int b = 0; b < 40; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            for (int i = 0; i < 256; ++i)
                d[i] = 0.1f * (float) std::sin (2.0 * M_PI * 440.0 * (b * 256 + i) / 48000.0);
        }
        proc.processBlock (buf, midi);
        CHECK (! vfatest::hasNanOrInf (buf.getReadPointer (0), 256));
        CHECK (! vfatest::hasNanOrInf (buf.getReadPointer (1), 256));
    }
}

VFA_TEST (Layout_mono_processes_clean)
{
    vfa::VfaProcessor proc;
    proc.setPlayConfigDetails (1, 1, 44100.0, 128);
    proc.prepareToPlay (44100.0, 128);
    juce::AudioBuffer<float> buf (1, 128);
    juce::MidiBuffer midi;
    for (int b = 0; b < 40; ++b)
    {
        auto* d = buf.getWritePointer (0);
        for (int i = 0; i < 128; ++i)
            d[i] = 0.1f * (float) std::sin (2.0 * M_PI * 330.0 * (b * 128 + i) / 44100.0);
        proc.processBlock (buf, midi);
        CHECK (! vfatest::hasNanOrInf (buf.getReadPointer (0), 128));
    }
}

VFA_TEST (Layout_support_matrix)
{
    vfa::VfaProcessor proc;
    juce::AudioProcessor::BusesLayout monoLayout, stereoLayout, mismatch;
    monoLayout.inputBuses.add (juce::AudioChannelSet::mono());
    monoLayout.outputBuses.add (juce::AudioChannelSet::mono());
    stereoLayout.inputBuses.add (juce::AudioChannelSet::stereo());
    stereoLayout.outputBuses.add (juce::AudioChannelSet::stereo());
    mismatch.inputBuses.add (juce::AudioChannelSet::mono());
    mismatch.outputBuses.add (juce::AudioChannelSet::stereo());

    CHECK (proc.checkBusesLayoutSupported (monoLayout));
    CHECK (proc.checkBusesLayoutSupported (stereoLayout));
    CHECK (! proc.checkBusesLayoutSupported (mismatch));
}

VFA_TEST (Layout_latency_reported_and_constant)
{
    vfa::VfaProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    const int lat = proc.getLatencySamples();
    // Transport centre delay (12 ms @ 48k = 576) plus oversampling latency.
    CHECK_MSG (lat >= 576 && lat < 1200, "latency out of expected range");

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    buf.clear();
    for (int b = 0; b < 20; ++b)
        proc.processBlock (buf, midi);
    CHECK (proc.getLatencySamples() == lat);   // stable without quality change
}
