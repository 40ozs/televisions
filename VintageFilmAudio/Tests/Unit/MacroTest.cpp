#include "../Harness/VfaTest.h"
#include "DSP/Core/MacroMath.h"

// Macro modulation math (ADR-003): bounds, neutral points, monotonicity.
// Macros never write parameters — they only shape effective values; these
// tests pin the contract the UI and docs describe.

using namespace vfa::dsp::macros;

VFA_TEST (Macro_fidelity_neutral_at_one)
{
    CHECK_NEAR (effectiveHfRollHz (12000.0f, 1.0f), 12000.0f, 1.0e-3);
    CHECK_NEAR (effectiveLfRollHz (50.0f, 1.0f), 50.0f, 1.0e-3);
    CHECK_NEAR (effectiveDistortion (0.4f, 1.0f), 0.4f, 1.0e-5);
}

VFA_TEST (Macro_fidelity_degrades_monotonically)
{
    float lastHf = 1.0e9f, lastDist = -1.0f, lastNoise = -1.0f;
    for (float f = 1.0f; f >= 0.0f; f -= 0.1f)
    {
        const float hf = effectiveHfRollHz (12000.0f, f);
        const float dist = effectiveDistortion (0.4f, f);
        const float noise = effectiveNoiseAmount (0.3f, 0.3f, f);
        CHECK (hf <= lastHf + 1.0e-4f);
        CHECK (dist >= lastDist - 1.0e-6f);
        CHECK (noise >= lastNoise - 1.0e-6f);
        lastHf = hf; lastDist = dist; lastNoise = noise;
    }
    // Worst case bandwidth narrowing is bounded (35 % of base).
    CHECK_NEAR (effectiveHfRollHz (12000.0f, 0.0f), 12000.0f * 0.35f, 1.0f);
}

VFA_TEST (Macro_all_outputs_bounded)
{
    for (float base = 0.0f; base <= 1.0f; base += 0.25f)
        for (float m = 0.0f; m <= 1.0f; m += 0.25f)
            for (float f = 0.0f; f <= 1.0f; f += 0.25f)
            {
                const float vals[] = {
                    effectiveDistortion (base, f),
                    effectiveNoiseAmount (base, m, f),
                    effectiveArtifactAmount (base, m),
                    effectiveCompression (base, m),
                    effectiveTransientSoften (base, m, f),
                };
                for (float v : vals)
                    CHECK_MSG (v >= 0.0f && v <= 1.0f, "macro output out of [0,1]");
                const float mid = effectiveMidShapeDb (base * 12.0f - 6.0f, m);
                CHECK (mid >= -6.0f && mid <= 6.0f);
            }
}

VFA_TEST (Macro_noise_neutral_point)
{
    // noiseMacro 0.3 with fidelity 1 must leave base values untouched.
    CHECK_NEAR (effectiveNoiseAmount (0.25f, 0.3f, 1.0f), 0.25f, 1.0e-5);
    CHECK_NEAR (effectiveArtifactAmount (0.25f, 0.3f), 0.25f, 1.0e-5);
    // Raising the macro raises the effective amount.
    CHECK (effectiveNoiseAmount (0.25f, 0.9f, 1.0f) > 0.25f);
    CHECK (effectiveArtifactAmount (0.25f, 0.9f) > 0.25f);
    // Zero macro silences.
    CHECK_NEAR (effectiveNoiseAmount (0.25f, 0.0f, 1.0f), 0.0f, 1.0e-5);
}

VFA_TEST (Macro_character_neutral_at_half)
{
    CHECK_NEAR (effectiveMidShapeDb (0.0f, 0.5f), 0.0f, 1.0e-5);
    CHECK_NEAR (effectiveCompression (0.3f, 0.5f), 0.3f, 1.0e-5);
    CHECK (effectiveCompression (0.3f, 1.0f) > effectiveCompression (0.3f, 0.0f));
}

VFA_TEST (Macro_generation_scale_sqrt_law)
{
    CHECK_NEAR (generationWowFlutterScale (0.0f), 1.0f, 1.0e-5);
    CHECK_NEAR (generationWowFlutterScale (3.0f), 2.0f, 1.0e-4);
    CHECK (generationWowFlutterScale (8.0f) > generationWowFlutterScale (4.0f));
}
