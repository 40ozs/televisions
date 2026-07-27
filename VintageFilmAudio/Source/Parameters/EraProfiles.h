#pragma once

#include <vector>
#include <utility>
#include "../DSP/Core/Enums.h"

namespace vfa::profiles
{

// A profile is a list of (parameterID, plain value) writes that establish a
// coherent base-parameter state for an Era x Medium x Condition selection
// (ADR-003: an explicit, undoable user gesture — macros are NOT written).
// Choice params use the index as value; bools use 0/1.
using ParamWrite = std::pair<const char*, float>;

std::vector<ParamWrite> profileFor (dsp::Era era, dsp::Medium medium, dsp::Condition condition);

} // namespace vfa::profiles
