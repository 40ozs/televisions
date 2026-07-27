#pragma once

// Stable parameter IDs — contract with Docs/PARAMETER_MANIFEST.md (never
// change an ID; see StateMigration for the policy).

namespace vfa::pid
{
inline constexpr auto era = "era";
inline constexpr auto medium = "medium";
inline constexpr auto condition = "condition";
inline constexpr auto character = "character";
inline constexpr auto fidelity = "fidelity";
inline constexpr auto generation = "generation";
inline constexpr auto genInteger = "genInteger";
inline constexpr auto genVariability = "genVariability";
inline constexpr auto artifacts = "artifacts";
inline constexpr auto noiseMacro = "noiseMacro";
inline constexpr auto deliveryCurve = "deliveryCurve";
inline constexpr auto mix = "mix";
inline constexpr auto inTrim = "inTrim";
inline constexpr auto outTrim = "outTrim";
inline constexpr auto autoGain = "autoGain";
inline constexpr auto safetyLimiter = "safetyLimiter";
inline constexpr auto audition = "audition";
inline constexpr auto quality = "quality";
inline constexpr auto seed = "seed";
inline constexpr auto deliveryOn = "deliveryOn";
inline constexpr auto mediumOn = "mediumOn";
inline constexpr auto transportOn = "transportOn";
inline constexpr auto genOn = "genOn";
inline constexpr auto noiseOn = "noiseOn";
inline constexpr auto dynOn = "dynOn";
inline constexpr auto reproOn = "reproOn";
inline constexpr auto delLfRoll = "delLfRoll";
inline constexpr auto delHfRoll = "delHfRoll";
inline constexpr auto delMidShape = "delMidShape";
inline constexpr auto delAcademyAmt = "delAcademyAmt";
inline constexpr auto delXcurveAmt = "delXcurveAmt";
inline constexpr auto delPlaybackSize = "delPlaybackSize";
inline constexpr auto medOpticalDist = "medOpticalDist";
inline constexpr auto medOpticalMode = "medOpticalMode";
inline constexpr auto medImageSpread = "medImageSpread";
inline constexpr auto medMagSat = "medMagSat";
inline constexpr auto medTapeSpeed = "medTapeSpeed";
inline constexpr auto medHeadBump = "medHeadBump";
inline constexpr auto medTransSoften = "medTransSoften";
inline constexpr auto medCrosstalk = "medCrosstalk";
inline constexpr auto wow = "wow";
inline constexpr auto wowRate = "wowRate";
inline constexpr auto flutter = "flutter";
inline constexpr auto flutterRate = "flutterRate";
inline constexpr auto drift = "drift";
inline constexpr auto scrape = "scrape";
inline constexpr auto stereoLink = "stereoLink";
inline constexpr auto transportQuality = "transportQuality";
inline constexpr auto nsHiss = "nsHiss";
inline constexpr auto nsCell = "nsCell";
inline constexpr auto nsBroadcast = "nsBroadcast";
inline constexpr auto nsHum = "nsHum";
inline constexpr auto nsHumFreq = "nsHumFreq";
inline constexpr auto nsHumHarm = "nsHumHarm";
inline constexpr auto nsBuzz = "nsBuzz";
inline constexpr auto nsCrackle = "nsCrackle";
inline constexpr auto nsDirt = "nsDirt";
inline constexpr auto nsDropout = "nsDropout";
inline constexpr auto nsProjector = "nsProjector";
inline constexpr auto nsPrintThrough = "nsPrintThrough";
inline constexpr auto nsDuck = "nsDuck";
inline constexpr auto nsSilence = "nsSilence";
inline constexpr auto nsWidth = "nsWidth";
inline constexpr auto dynAgc = "dynAgc";
inline constexpr auto dynComp = "dynComp";
inline constexpr auto dynLimit = "dynLimit";
inline constexpr auto dynRelChar = "dynRelChar";
inline constexpr auto dynDialog = "dynDialog";
inline constexpr auto dynPump = "dynPump";
inline constexpr auto reproMono = "reproMono";
inline constexpr auto monoLaw = "monoLaw";
inline constexpr auto reproWidth = "reproWidth";
inline constexpr auto reproSpeaker = "reproSpeaker";
} // namespace vfa::pid
