#pragma once

#include <algorithm>
#include <vector>
#include "../Core/Enums.h"
#include "../Core/Filters.h"
#include "GenerationModel.h"

namespace vfa::dsp
{

// TEST-ONLY reference for the generation-loss model (ADR-007, DSP_SPEC §6).
//
// Literally applies the simplified single-transfer stage N integer times:
// each stage = one-pole LP at prototypeCutoffHz (medium) + the same mild
// +1 dB saturator drive increment (level-normalized, matching the per-
// generation slope of GenerationModel::driveLiftDb) + one transient-softener
// increment (matching the optimized model's 0.08-per-generation slope).
//
// It exists solely so GenerationTest can validate the optimized
// GenerationModel's magnitude trend against ground truth. It allocates in
// prepare(), is NOT realtime-safe, and must never be used by the engine.
class ExactReferenceChain
{
public:
    void prepare (double sampleRate, Medium medium, int generations)
    {
        stages.assign ((size_t) std::max (generations, 0), Stage());
        const float f0 = GenerationModel::prototypeCutoffHz (medium);
        for (auto& s : stages)
        {
            s.lp.setCutoff (f0, sampleRate);
            s.softener.prepare (sampleRate);
        }
    }

    void reset()
    {
        for (auto& s : stages)
        {
            s.lp.reset();
            s.softener.reset();
        }
    }

    // In-place mono processing (measurement probes are mono).
    void process (float* x, int n)
    {
        for (auto& s : stages)
            for (int i = 0; i < n; ++i)
            {
                float v = s.lp.process (x[i]);
                v = fastTanh (v * kStageDrive) / kStageDrive;   // mild per-stage sat
                x[i] = s.softener.process (v, kSoftenPerStage);
            }
    }

private:
    struct Stage
    {
        OnePoleLP lp;
        TransientSoftener softener;
    };

    static constexpr float kStageDrive = 1.1220185f;    // dbToGain (1.0f)
    static constexpr float kSoftenPerStage = 0.08f;     // matches 0.08 * N

    std::vector<Stage> stages;
};

} // namespace vfa::dsp
