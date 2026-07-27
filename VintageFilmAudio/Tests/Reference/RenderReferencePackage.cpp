// Subjective test package renderer (TEST_PLAN §Reference, PRD §17 support).
// Generates a synthetic, license-free test corpus and renders it through:
//   A) bypass, B) selected factory presets, C) an EQ-only approximation
//   (delivery curve alone), D) a generic-saturation approximation (magnetic
//   stage alone) — level-matched, with randomized blind labels for ABX-style
//   listening. Everything is seed-deterministic: re-running reproduces the
//   package bit-exactly.
//
// Usage: VfaRenderPackage [outputDir]   (default: TestOutput/SubjectivePackage)

#include <juce_audio_formats/juce_audio_formats.h>
#include "Plugin/PluginProcessor.h"
#include "Parameters/ParameterIDs.h"
#include "DSP/Core/Rng.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 512;
constexpr double kSeconds = 6.0;

using Buffer = juce::AudioBuffer<float>;

// ---- synthetic corpus (no licensed material; deterministic) --------------
Buffer makeSignal (const juce::String& name)
{
    const int n = (int) (kSampleRate * kSeconds);
    Buffer buf (2, n);
    buf.clear();
    vfa::dsp::Rng rng (juce::DefaultHashFunctions::generateHash (name, 1 << 30) + 7);

    auto* l = buf.getWritePointer (0);
    auto* r = buf.getWritePointer (1);

    if (name == "dialogue_like")
    {
        // Glottal-ish pulse train through moving formant resonators, gated
        // into word-like bursts.
        vfa::dsp::Biquad f1, f2;
        double phase = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double t = i / kSampleRate;
            const float f0 = 110.0f + 20.0f * (float) std::sin (2.0 * M_PI * 0.7 * t);
            phase += f0 / kSampleRate;
            if (phase >= 1.0) phase -= 1.0;
            const float pulse = phase < 0.1 ? (float) (1.0 - phase / 0.1) : 0.0f;
            if ((i & 1023) == 0)
            {
                const float form1 = 500.0f + 300.0f * rng.next01();
                const float form2 = 1500.0f + 900.0f * rng.next01();
                f1.bandpass (kSampleRate, form1, 4.0f);
                f2.bandpass (kSampleRate, form2, 6.0f);
            }
            const float gate = std::fmod (t, 1.4) < 0.9 ? 1.0f : 0.0f;
            const float s = (f1.process (pulse) + 0.6f * f2.process (pulse)) * gate * 0.5f;
            l[i] = s; r[i] = s;
        }
    }
    else if (name == "sibilant_bursts")
    {
        vfa::dsp::Biquad hp;
        hp.highpass (kSampleRate, 5000.0f, 0.9f);
        for (int i = 0; i < n; ++i)
        {
            const double t = i / kSampleRate;
            const float gate = std::fmod (t, 0.5) < 0.12 ? 1.0f : 0.0f;
            const float s = hp.process (rng.nextBipolar()) * gate * 0.35f;
            l[i] = s; r[i] = s;
        }
    }
    else if (name == "music_like")
    {
        const float chord[4] = { 130.81f, 196.0f, 261.63f, 329.63f };
        double phases[4] = {};
        for (int i = 0; i < n; ++i)
        {
            const double t = i / kSampleRate;
            float s = 0.0f;
            for (int v = 0; v < 4; ++v)
            {
                phases[v] += chord[v] * (v == 3 && t > 3.0 ? 1.1225 : 1.0) / kSampleRate;
                s += 0.11f * (float) std::sin (2.0 * M_PI * phases[v])
                   + 0.03f * (float) std::sin (4.0 * M_PI * phases[v]);
            }
            const float trem = 0.8f + 0.2f * (float) std::sin (2.0 * M_PI * 5.0 * t);
            l[i] = s * trem;
            r[i] = s * trem * 0.9f;
        }
    }
    else if (name == "transients")
    {
        for (int i = 0; i < n; ++i)
        {
            const int period = (int) (kSampleRate * 0.4);
            const int ph = i % period;
            const float s = ph < 24 ? (1.0f - ph / 24.0f) * 0.7f * (ph % 2 == 0 ? 1.0f : -0.6f) : 0.0f;
            l[i] = s; r[i] = s * 0.85f;
        }
    }
    else // room_tone
    {
        vfa::dsp::OnePoleLP lp;
        lp.setCutoff (400.0f, kSampleRate);
        for (int i = 0; i < n; ++i)
        {
            const float s = lp.process (rng.nextBipolar()) * 0.05f;
            l[i] = s;
            r[i] = lp.process (rng.nextBipolar()) * 0.05f;
        }
    }
    return buf;
}

void setPlain (vfa::VfaProcessor& proc, const char* pid, float plain)
{
    if (auto* p = proc.parameters().getParameter (pid))
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

Buffer renderThrough (vfa::VfaProcessor& proc, const Buffer& input)
{
    Buffer out (2, input.getNumSamples());
    for (int ch = 0; ch < 2; ++ch)
        out.copyFrom (ch, 0, input, ch, 0, input.getNumSamples());
    proc.setPlayConfigDetails (2, 2, kSampleRate, kBlock);
    proc.prepareToPlay (kSampleRate, kBlock);
    juce::MidiBuffer midi;
    for (int pos = 0; pos < out.getNumSamples(); pos += kBlock)
    {
        const int len = std::min (kBlock, out.getNumSamples() - pos);
        Buffer view (out.getArrayOfWritePointers(), 2, pos, len);
        proc.processBlock (view, midi);
    }
    return out;
}

void levelMatch (Buffer& buf, double targetRmsDb)
{
    double acc = 0.0;
    const int n = buf.getNumSamples();
    for (int ch = 0; ch < 2; ++ch)
    {
        const auto* d = buf.getReadPointer (ch);
        for (int i = 0; i < n; ++i) acc += (double) d[i] * d[i];
    }
    const double rms = std::sqrt (acc / (2.0 * n));
    if (rms < 1.0e-9) return;
    const float g = (float) (std::pow (10.0, targetRmsDb / 20.0) / rms);
    buf.applyGain (std::min (g, 16.0f));
}

void writeWav (const juce::File& file, const Buffer& buf)
{
    file.deleteFile();
    juce::WavAudioFormat format;
    auto stream = file.createOutputStream();
    if (stream == nullptr) return;
    if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (
            format.createWriterFor (stream.get(), kSampleRate, 2, 24, {}, 0)))
    {
        stream.release();   // writer owns it now
        writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    }
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File outDir = argc > 1
        ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
        : juce::File (VFA_REPO_ROOT).getChildFile ("TestOutput/SubjectivePackage");
    outDir.createDirectory();

    const char* signals[] = { "dialogue_like", "sibilant_bursts", "music_like",
                              "transients", "room_tone" };
    // Renditions: bypass, four representative presets, EQ-only, generic-sat.
    struct Rendition { const char* label; int preset; int mode; };
    // mode 0=preset/bypass, 1=EQ-only approximation, 2=generic-sat approximation
    const Rendition renditions[] = {
        { "bypass", -1, 0 },
        { "preset_1950s_theater", 0, 0 },
        { "preset_kinescope", 2, 0 },
        { "preset_1977_optical_stereo", 8, 0 },
        { "preset_offair", 15, 0 },
        { "eq_only_approx", 0, 1 },
        { "generic_sat_approx", 0, 2 },
    };

    // Blind labels: deterministic shuffle per signal (seeded).
    juce::String manifest = "{\n  \"sampleRate\": 48000,\n  \"note\": \"labels randomized per signal; key below\",\n  \"entries\": [\n";
    bool firstEntry = true;
    vfa::dsp::Rng labelRng (20260726);

    for (const auto* signalName : signals)
    {
        const auto input = makeSignal (signalName);
        std::vector<int> order (std::size (renditions));
        for (size_t i = 0; i < order.size(); ++i) order[i] = (int) i;
        for (size_t i = order.size(); i > 1; --i)
            std::swap (order[i - 1], order[labelRng.nextU64() % i]);

        for (size_t slot = 0; slot < order.size(); ++slot)
        {
            const auto& rendition = renditions[(size_t) order[slot]];
            Buffer out;
            if (juce::String (rendition.label) == "bypass")
            {
                out = input;
            }
            else
            {
                vfa::VfaProcessor proc;
                namespace id = vfa::pid;
                if (rendition.preset >= 0)
                    proc.setCurrentProgram (rendition.preset);
                setPlain (proc, id::seed, 424242.0f > 99999.0f ? 42424.0f : 42424.0f);
                if (rendition.mode == 1)
                {
                    // EQ-only: delivery curve alone (the "generic EQ" strawman).
                    for (const char* pid : { id::mediumOn, id::transportOn, id::genOn,
                                             id::noiseOn, id::dynOn, id::reproOn })
                        setPlain (proc, pid, 0.0f);
                }
                else if (rendition.mode == 2)
                {
                    // Generic saturation only (the "tape plugin" strawman).
                    setPlain (proc, id::medium, 3.0f);   // magnetic
                    for (const char* pid : { id::deliveryOn, id::transportOn, id::genOn,
                                             id::noiseOn, id::dynOn, id::reproOn })
                        setPlain (proc, pid, 0.0f);
                }
                out = renderThrough (proc, input);
            }
            levelMatch (out, -20.0);

            const auto blind = juce::String::formatted ("%s_stim%02d.wav", signalName, (int) slot);
            writeWav (outDir.getChildFile (blind), out);
            manifest << (firstEntry ? "" : ",\n")
                     << "    {\"file\": \"" << blind << "\", \"signal\": \"" << signalName
                     << "\", \"rendition\": \"" << rendition.label << "\"}";
            firstEntry = false;
        }
    }
    manifest << "\n  ]\n}\n";
    outDir.getChildFile ("manifest.json").replaceWithText (manifest);
    std::printf ("Subjective package written to %s\n", outDir.getFullPathName().toRawUTF8());
    return 0;
}
