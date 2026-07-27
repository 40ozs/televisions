#pragma once

#include <algorithm>
#include <cmath>
#include "ParamSnapshot.h"

// Macro modulation math (ADR-003): pure functions combining *base* parameter
// values with macro positions into *effective* snapshot values. Macros never
// write parameters; this is the single place where their influence is
// defined, so it is unit-testable in isolation (Unit/MacroTest).
//
// Convention: each macro has a neutral position where effective == base.
//   Fidelity  neutral = 1.0 (lowering fidelity degrades)
//   Character neutral = 0.5 (0 = optical/dark emphasis, 1 = magnetic/forward)
//   Artifacts neutral = its own value (scales event-type amounts directly)
//   Noise     neutral = its own value (scales noise-type amounts directly)
//   Generation is itself a base parameter (macro == parameter).

namespace vfa::dsp::macros
{

struct MacroValues
{
    float character = 0.5f;
    float fidelity  = 0.7f;
    float artifacts = 0.3f;
    float noise     = 0.3f;
};

inline float clamp01 (float v) noexcept { return std::clamp (v, 0.0f, 1.0f); }

// Fidelity < 1 narrows bandwidth: scales the advanced HF roll-off down to
// 35 % and raises LF roll-off up to 3x at fidelity 0.
inline float effectiveHfRollHz (float baseHz, float fidelity) noexcept
{
    const float k = 0.35f + 0.65f * clamp01 (fidelity);
    return baseHz * k;
}
inline float effectiveLfRollHz (float baseHz, float fidelity) noexcept
{
    const float k = 1.0f + 2.0f * (1.0f - clamp01 (fidelity));
    return baseHz * k;
}

// Fidelity scales distortion/transfer-loss upward as it drops (up to +60 %),
// and noise upward (up to +12 dB expressed as a 0..1 amount multiplier).
inline float effectiveDistortion (float base, float fidelity) noexcept
{
    return clamp01 (base * (1.0f + 0.6f * (1.0f - clamp01 (fidelity))));
}
inline float effectiveNoiseAmount (float base, float noiseMacro, float fidelity) noexcept
{
    const float fidelityLift = 1.0f + 0.5f * (1.0f - clamp01 (fidelity));
    // noiseMacro 0.3 is the neutral point where factory defaults sit.
    const float macroScale = clamp01 (noiseMacro) / 0.3f;
    return clamp01 (base * macroScale * fidelityLift);
}
inline float effectiveArtifactAmount (float base, float artifactsMacro) noexcept
{
    const float macroScale = clamp01 (artifactsMacro) / 0.3f;
    return clamp01 (base * macroScale);
}

// Character: 0 = darker/rounder (optical-leaning), 1 = forward/denser
// (magnetic/broadcast-leaning). Affects mid shape and dynamics density.
inline float effectiveMidShapeDb (float baseDb, float character) noexcept
{
    return std::clamp (baseDb + (clamp01 (character) - 0.5f) * 4.0f, -6.0f, 6.0f);
}
inline float effectiveCompression (float base, float character) noexcept
{
    return clamp01 (base * (0.7f + 0.6f * clamp01 (character)));
}
inline float effectiveTransientSoften (float base, float character, float fidelity) noexcept
{
    const float dark = 1.0f - clamp01 (character);
    return clamp01 (base + 0.25f * dark * (1.0f - 0.5f * clamp01 (fidelity)));
}

// Generation macro contributions (ADR-007): wow/flutter and softening scale
// with sqrt(N); saturation and noise contributions computed in the modules.
inline float generationWowFlutterScale (float generations) noexcept
{
    return std::sqrt (1.0f + std::max (0.0f, generations));
}

} // namespace vfa::dsp::macros
