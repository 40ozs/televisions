#pragma once

namespace vfa::dsp
{

struct StreamSpec
{
    double sampleRate = 48000.0;
    int maxBlockSize  = 512;
    int numChannels   = 2;
};

} // namespace vfa::dsp
