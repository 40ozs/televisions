#pragma once

// Enum values MUST match the choice-parameter orderings in
// Docs/PARAMETER_MANIFEST.md (checked by ParameterManifestTest).

namespace vfa::dsp
{

enum class Era
{
    early1950s, late1950s, early1960s, late1960s,
    early1970s, late1970s, early1980s, late1980s
};
constexpr int numEras = 8;

enum class Medium
{
    clean, opticalMono, opticalStereo, magneticFilm, fieldTape,
    kinescope, broadcastMono, broadcastStereo, consumer
};
constexpr int numMediums = 9;

enum class Condition
{
    master, releasePrint, broadcast, offAir,
    workprint, wornArchive, multiGeneration, damaged
};
constexpr int numConditions = 8;

enum class CurveMode
{
    neutral, academy, xCurve, xCurveSmallRoom,
    earlyTv, kinescope, broadcastMono, lateTv
};
constexpr int numCurveModes = 8;

enum class OpticalMode { variableArea, variableDensity };
enum class TapeSpeed { ips3_75, ips7_5, ips15, ips30 };
enum class TransportQuality { lab, studio, portable, worn, damaged };
enum class Quality { eco, standard, high };
enum class AuditionMode { normal, artifactsOnly, noiseMuted };
enum class MonoMode { stereo, narrow, mono };
enum class MonoLaw { minus3dB, minus4_5dB, minus6dB };

inline float tapeSpeedIps (TapeSpeed s) noexcept
{
    switch (s)
    {
        case TapeSpeed::ips3_75: return 3.75f;
        case TapeSpeed::ips7_5:  return 7.5f;
        case TapeSpeed::ips15:   return 15.0f;
        case TapeSpeed::ips30:   return 30.0f;
    }
    return 15.0f;
}

} // namespace vfa::dsp
