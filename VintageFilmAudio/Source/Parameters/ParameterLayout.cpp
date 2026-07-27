#include "ParameterLayout.h"
#include "ParameterIDs.h"

namespace vfa::params
{

const std::vector<ParamMeta>& allParams()
{
    using T = ParamMeta::Type;
    namespace id = vfa::pid;
    static const std::vector<ParamMeta> table = {
        { id::era, "Era", T::Choice, 0, 0, 2, 1, "",
          "Early 1950s|Late 1950s|Early 1960s|Late 1960s|Early 1970s|Late 1970s|Early 1980s|Late 1980s", 0, true },
        { id::medium, "Medium", T::Choice, 0, 0, 3, 1, "",
          "Clean|Optical Mono|Optical Stereo|Magnetic Film|Field Tape|Kinescope|Broadcast Mono|Broadcast Stereo|Consumer", 0, true },
        { id::condition, "Condition", T::Choice, 0, 0, 1, 1, "",
          "Master|Release Print|Broadcast|Off-Air|Workprint|Worn Archive|Multi-Generation|Damaged", 0, true },
        { id::character, "Character", T::Float, 0, 1, 0.5f, 1, "", nullptr, 50, true },
        { id::fidelity, "Fidelity", T::Float, 0, 1, 0.7f, 1, "", nullptr, 50, true },
        { id::generation, "Generation", T::Float, 0, 8, 1, 1, "gens", nullptr, 100, true },
        { id::genInteger, "Exact Generations", T::Bool, 0, 1, 0, 1, "", nullptr, 0, true },
        { id::genVariability, "Gen Variability", T::Float, 0, 1, 0.3f, 1, "", nullptr, 50, true },
        { id::artifacts, "Artifacts", T::Float, 0, 1, 0.3f, 1, "", nullptr, 50, true },
        { id::noiseMacro, "Noise", T::Float, 0, 1, 0.3f, 1, "", nullptr, 50, true },
        { id::deliveryCurve, "Delivery Curve", T::Choice, 0, 0, 1, 1, "",
          "Neutral|Academy|X-Curve|X-Curve Small Room|Early TV|Kinescope|Broadcast Mono|Late TV", 0, true },
        { id::mix, "Mix", T::Float, 0, 100, 100, 1, "%", nullptr, 20, true },
        { id::inTrim, "Input Trim", T::Float, -24, 24, 0, 1, "dB", nullptr, 20, true },
        { id::outTrim, "Output Trim", T::Float, -24, 24, 0, 1, "dB", nullptr, 20, true },
        { id::autoGain, "Auto Gain", T::Bool, 0, 1, 0, 1, "", nullptr, 0, true },
        { id::safetyLimiter, "Safety Limiter", T::Bool, 0, 1, 1, 1, "", nullptr, 0, true },
        { id::audition, "Audition", T::Choice, 0, 0, 0, 1, "",
          "Normal|Artifacts Only|Noise Muted", 0, true },
        { id::quality, "Quality", T::Choice, 0, 0, 1, 1, "", "Eco|Standard|High", 0, false },
        { id::seed, "Random Seed", T::Int, 0, 99999, 0, 1, "", nullptr, 0, false },
        { id::deliveryOn, "Delivery Enable", T::Bool, 0, 1, 1, 1, "", nullptr, 0, true },
        { id::mediumOn, "Medium Enable", T::Bool, 0, 1, 1, 1, "", nullptr, 0, true },
        { id::transportOn, "Transport Enable", T::Bool, 0, 1, 1, 1, "", nullptr, 0, true },
        { id::genOn, "Generation Enable", T::Bool, 0, 1, 1, 1, "", nullptr, 0, true },
        { id::noiseOn, "Noise Enable", T::Bool, 0, 1, 1, 1, "", nullptr, 0, true },
        { id::dynOn, "Dynamics Enable", T::Bool, 0, 1, 1, 1, "", nullptr, 0, true },
        { id::reproOn, "Reproduction Enable", T::Bool, 0, 1, 1, 1, "", nullptr, 0, true },
        { id::delLfRoll, "LF Roll-Off", T::Float, 20, 300, 50, 0.4f, "Hz", nullptr, 50, true },
        { id::delHfRoll, "HF Roll-Off", T::Float, 2000, 20000, 12000, 0.4f, "Hz", nullptr, 50, true },
        { id::delMidShape, "Midrange Shape", T::Float, -6, 6, 0, 1, "dB", nullptr, 50, true },
        { id::delAcademyAmt, "Academy Amount", T::Float, 0, 1, 1, 1, "", nullptr, 50, true },
        { id::delXcurveAmt, "X-Curve Amount", T::Float, 0, 1, 1, 1, "", nullptr, 50, true },
        { id::delPlaybackSize, "Playback Size", T::Float, 0, 1, 0.5f, 1, "", nullptr, 50, true },
        { id::medOpticalDist, "Optical Distortion", T::Float, 0, 1, 0.4f, 1, "", nullptr, 50, true },
        { id::medOpticalMode, "Optical Mode", T::Choice, 0, 0, 0, 1, "",
          "Variable Area|Variable Density", 0, true },
        { id::medImageSpread, "Image Spread", T::Float, 0, 1, 0.4f, 1, "", nullptr, 50, true },
        { id::medMagSat, "Magnetic Saturation", T::Float, 0, 1, 0.4f, 1, "", nullptr, 50, true },
        { id::medTapeSpeed, "Tape Speed", T::Choice, 0, 0, 2, 1, "",
          "3.75 ips|7.5 ips|15 ips|30 ips", 0, true },
        { id::medHeadBump, "Head Bump", T::Float, 0, 1, 0.5f, 1, "", nullptr, 50, true },
        { id::medTransSoften, "Transient Softening", T::Float, 0, 1, 0.3f, 1, "", nullptr, 50, true },
        { id::medCrosstalk, "Crosstalk", T::Float, 0, 1, 0.3f, 1, "", nullptr, 50, true },
        { id::wow, "Wow", T::Float, 0, 1, 0.25f, 1, "", nullptr, 50, true },
        { id::wowRate, "Wow Rate", T::Float, 0.2f, 4, 0.65f, 0.5f, "Hz", nullptr, 50, true },
        { id::flutter, "Flutter", T::Float, 0, 1, 0.25f, 1, "", nullptr, 50, true },
        { id::flutterRate, "Flutter Rate", T::Float, 8, 40, 24, 0.5f, "Hz", nullptr, 50, true },
        { id::drift, "Drift", T::Float, 0, 1, 0.2f, 1, "", nullptr, 50, true },
        { id::scrape, "Scrape", T::Float, 0, 1, 0.1f, 1, "", nullptr, 50, true },
        { id::stereoLink, "Transport Stereo Link", T::Bool, 0, 1, 1, 1, "", nullptr, 0, true },
        { id::transportQuality, "Transport Quality", T::Choice, 0, 0, 1, 1, "",
          "Lab|Studio|Portable|Worn|Damaged", 0, true },
        { id::nsHiss, "Hiss", T::Float, 0, 1, 0.3f, 1, "", nullptr, 50, true },
        { id::nsCell, "Cell Noise", T::Float, 0, 1, 0.2f, 1, "", nullptr, 50, true },
        { id::nsBroadcast, "Broadcast Noise", T::Float, 0, 1, 0, 1, "", nullptr, 50, true },
        { id::nsHum, "Hum", T::Float, 0, 1, 0.15f, 1, "", nullptr, 50, true },
        { id::nsHumFreq, "Hum Frequency", T::Choice, 0, 0, 1, 1, "", "50 Hz|60 Hz", 0, true },
        { id::nsHumHarm, "Hum Harmonics", T::Float, 0, 1, 0.3f, 1, "", nullptr, 50, true },
        { id::nsBuzz, "Sync Buzz", T::Float, 0, 1, 0, 1, "", nullptr, 50, true },
        { id::nsCrackle, "Crackle", T::Float, 0, 1, 0.2f, 1, "", nullptr, 50, true },
        { id::nsDirt, "Dirt Clicks", T::Float, 0, 1, 0.15f, 1, "", nullptr, 50, true },
        { id::nsDropout, "Dropouts", T::Float, 0, 1, 0, 1, "", nullptr, 50, true },
        { id::nsProjector, "Projector", T::Float, 0, 1, 0.1f, 1, "", nullptr, 50, true },
        { id::nsPrintThrough, "Print-Through", T::Float, 0, 1, 0.1f, 1, "", nullptr, 50, true },
        { id::nsDuck, "Noise Ducking", T::Float, 0, 1, 0.3f, 1, "", nullptr, 50, true },
        { id::nsSilence, "Noise In Silence", T::Bool, 0, 1, 1, 1, "", nullptr, 0, true },
        { id::nsWidth, "Noise Width", T::Float, 0, 1, 0.5f, 1, "", nullptr, 50, true },
        { id::dynAgc, "AGC", T::Float, 0, 1, 0.2f, 1, "", nullptr, 50, true },
        { id::dynComp, "Compression", T::Float, 0, 1, 0.3f, 1, "", nullptr, 50, true },
        { id::dynLimit, "Limiting", T::Float, 0, 1, 0.3f, 1, "", nullptr, 50, true },
        { id::dynRelChar, "Release Character", T::Float, 0, 1, 0.5f, 1, "", nullptr, 50, true },
        { id::dynDialog, "Dialogue Focus", T::Float, 0, 1, 0.2f, 1, "", nullptr, 50, true },
        { id::dynPump, "Pumping", T::Float, 0, 1, 0.1f, 1, "", nullptr, 50, true },
        { id::reproMono, "Mono Mode", T::Choice, 0, 0, 0, 1, "", "Stereo|Narrow|Mono", 0, true },
        { id::monoLaw, "Mono Sum Law", T::Choice, 0, 0, 0, 1, "", "-3 dB|-4.5 dB|-6 dB", 0, true },
        { id::reproWidth, "Width", T::Float, 0, 1, 1, 1, "", nullptr, 50, true },
        { id::reproSpeaker, "Small Speaker", T::Float, 0, 1, 0, 1, "", nullptr, 50, true },
    };
    return table;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& p : allParams())
    {
        const ParameterID pid { p.id, 1 };
        switch (p.type)
        {
            case ParamMeta::Type::Float:
            {
                NormalisableRange<float> range (p.min, p.max, 0.0f, p.skew);
                auto attr = AudioParameterFloatAttributes().withLabel (p.unit)
                                .withAutomatable (p.automatable);
                layout.add (std::make_unique<AudioParameterFloat> (pid, p.displayName, range, p.def, attr));
                break;
            }
            case ParamMeta::Type::Choice:
            {
                StringArray options;
                options.addTokens (String (p.choices), "|", "");
                auto attr = AudioParameterChoiceAttributes().withAutomatable (p.automatable);
                layout.add (std::make_unique<AudioParameterChoice> (pid, p.displayName, options, (int) p.def, attr));
                break;
            }
            case ParamMeta::Type::Bool:
            {
                auto attr = AudioParameterBoolAttributes().withAutomatable (p.automatable);
                layout.add (std::make_unique<AudioParameterBool> (pid, p.displayName, p.def >= 0.5f, attr));
                break;
            }
            case ParamMeta::Type::Int:
            {
                auto attr = AudioParameterIntAttributes().withAutomatable (p.automatable);
                layout.add (std::make_unique<AudioParameterInt> (pid, p.displayName, (int) p.min, (int) p.max, (int) p.def, attr));
                break;
            }
        }
    }
    return layout;
}

} // namespace vfa::params
