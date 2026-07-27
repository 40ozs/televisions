// STUB — replaced in Phase 3 (real optical model per DSP_SPEC §4).
#include "OpticalModel.h"

namespace vfa::dsp
{
namespace
{
class StubImageSpread : public IImageSpreadStage
{
public:
    void prepare (const StreamSpec&) override {}
    void reset() override {}
    void process (float*, int, int, float) override {}
};
}

OpticalModel::OpticalModel() : imageSpread (std::make_unique<StubImageSpread>()) {}
OpticalModel::~OpticalModel() = default;

void OpticalModel::prepare (const StreamSpec& spec)
{
    streamSpec = spec;
    channels.assign ((size_t) spec.numChannels, {});
    for (auto& c : channels) { c.peakEnv.prepare (spec.sampleRate, 0.3f, 30.0f); c.dc.prepare (spec.sampleRate); }
    imageSpread->prepare (spec);
    os.prepare (spec);
    scratch.setSize (spec.numChannels, spec.maxBlockSize * 4, false, false, true);
}

void OpticalModel::reset()
{
    for (auto& c : channels) { c.slit1.reset(); c.slit2.reset(); c.preEmph.reset(); c.deEmph.reset(); c.peakEnv.reset(); c.dc.reset(); }
    imageSpread->reset();
    os.reset();
}

void OpticalModel::process (juce::AudioBuffer<float>&, const ParamSnapshot&, float, float)
{
    // passthrough stub
}
} // namespace vfa::dsp
