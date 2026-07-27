#pragma once

#include <cstdint>
#include "Enums.h"

namespace vfa::dsp
{

// Per-block snapshot of *effective* parameter values: base parameter values
// combined with macro modulation (ADR-003). Built once per processBlock on
// the audio thread from lock-free atomics; plain data, no methods with state.
// Modules do their own smoothing of fields that need it.
struct ParamSnapshot
{
    // Global
    float inTrimDb  = 0.0f;
    float outTrimDb = 0.0f;
    float mix01     = 1.0f;
    bool  autoGain      = false;
    bool  safetyLimiter = true;
    AuditionMode audition = AuditionMode::normal;
    Quality quality = Quality::standard;
    uint64_t seed   = 1;          // resolved stream seed (never 0)

    bool deliveryOn = true, mediumOn = true, transportOn = true,
         genOn = true, noiseOn = true, dynOn = true, reproOn = true;

    // Context
    Era era = Era::early1960s;
    Medium medium = Medium::magneticFilm;
    Condition condition = Condition::releasePrint;
    CurveMode curveMode = CurveMode::academy;

    // Delivery (advanced)
    float delLfRollHz   = 50.0f;
    float delHfRollHz   = 12000.0f;
    float delMidShapeDb = 0.0f;
    float academyAmt    = 1.0f;
    float xcurveAmt     = 1.0f;
    float playbackSize  = 0.5f;

    // Medium
    float opticalDist = 0.4f;
    float imageSpread = 0.4f;
    float magSat      = 0.4f;
    float headBump    = 0.5f;
    float transSoften = 0.3f;
    float crosstalk   = 0.3f;
    OpticalMode opticalMode = OpticalMode::variableArea;
    TapeSpeed tapeSpeed = TapeSpeed::ips15;

    // Transport
    float wow = 0.25f, wowRateHz = 0.65f;
    float flutter = 0.25f, flutterRateHz = 24.0f;
    float drift = 0.2f, scrape = 0.1f;
    bool  stereoLink = true;
    TransportQuality transportQuality = TransportQuality::studio;

    // Generation
    float generations = 1.0f;     // 0..8, effective (macro-combined)
    bool  genInteger = false;
    float genVariability = 0.3f;

    // Noise
    float nsHiss = 0.3f, nsCell = 0.2f, nsBroadcast = 0.0f;
    float nsHum = 0.15f, nsHumHarm = 0.3f, nsBuzz = 0.0f;
    float nsCrackle = 0.2f, nsDirt = 0.15f, nsDropout = 0.0f;
    float nsProjector = 0.1f, nsPrintThrough = 0.1f;
    float nsDuck = 0.3f, nsWidth = 0.5f;
    bool  nsSilence = true;
    float humFreqHz = 60.0f;

    // Dynamics
    float agc = 0.2f, comp = 0.3f, limit = 0.3f;
    float relChar = 0.5f, dialog = 0.2f, pump = 0.1f;

    // Reproduction
    MonoMode monoMode = MonoMode::stereo;
    MonoLaw  monoLaw  = MonoLaw::minus3dB;
    float width = 1.0f;
    float smallSpeaker = 0.0f;
};

} // namespace vfa::dsp
