#pragma once

#include <array>
#include <vector>
#include "../Core/Enums.h"

// Tabulated delivery-curve magnitude targets (dB re 1 kHz) from
// Docs/DSP_SPEC.md §2.1. Single source of truth: the DSP reads these tables
// to design filters and the response tests read them to assert tolerances.
// Values marked HIST-APPROX in the spec are approximations, not measurements.

namespace vfa::dsp::curves
{

struct BreakPoint { float hz; float dB; };

// Anchor points carry the tight tolerance from the spec; others the loose one.
struct TargetPoint { float hz; float dB; float toleranceDb; };

inline const std::vector<BreakPoint>& breakpointsFor (CurveMode mode)
{
    static const std::vector<BreakPoint> neutral = { { 20.0f, 0.0f }, { 20000.0f, 0.0f } };
    static const std::vector<BreakPoint> academy = {
        { 20.0f, -12.0f }, { 31.5f, -9.0f }, { 40.0f, -7.0f }, { 63.0f, -4.0f },
        { 100.0f, 0.0f }, { 250.0f, 0.0f }, { 1000.0f, 0.0f }, { 1600.0f, 0.0f },
        { 2500.0f, -2.5f }, { 4000.0f, -7.0f }, { 5000.0f, -10.0f },
        { 6300.0f, -13.5f }, { 8000.0f, -18.0f }, { 10000.0f, -24.0f },
        { 16000.0f, -30.0f }, { 20000.0f, -30.0f } };
    // X-Curve-inspired: flat to 2 kHz, -3 dB/oct above; -3 dB/oct below 50 Hz.
    // NOTE (historical accuracy): SMPTE-202-style X-Curve is a *playback room
    // calibration target*; applying it as a transfer response emulates the
    // audible result of the theatrical chain. Documented in DSP_SPEC / manual.
    static const std::vector<BreakPoint> xcurve = {
        { 20.0f, -3.97f }, { 50.0f, 0.0f }, { 2000.0f, 0.0f },
        { 4000.0f, -3.0f }, { 8000.0f, -6.0f }, { 16000.0f, -9.0f }, { 20000.0f, -9.97f } };
    static const std::vector<BreakPoint> xcurveSmall = {
        { 20.0f, -1.99f }, { 50.0f, 0.0f }, { 2000.0f, 0.0f },
        { 4000.0f, -1.5f }, { 8000.0f, -3.0f }, { 16000.0f, -4.5f }, { 20000.0f, -4.99f } };
    static const std::vector<BreakPoint> earlyTv = {
        { 20.0f, -14.0f }, { 50.0f, -9.0f }, { 80.0f, -6.0f }, { 150.0f, 0.0f },
        { 1000.0f, 0.0f }, { 2000.0f, 2.0f }, { 3000.0f, 2.0f }, { 4000.0f, 0.0f },
        { 6000.0f, -6.0f }, { 10000.0f, -18.0f }, { 20000.0f, -30.0f } };
    static const std::vector<BreakPoint> kinescope = {
        { 20.0f, -12.0f }, { 60.0f, -6.0f }, { 100.0f, -3.0f }, { 120.0f, 0.0f },
        { 1000.0f, 0.0f }, { 1500.0f, 1.5f }, { 2500.0f, 1.5f }, { 3500.0f, 0.0f },
        { 6000.0f, -10.0f }, { 9000.0f, -24.0f }, { 20000.0f, -36.0f } };
    static const std::vector<BreakPoint> broadcastMono = {
        { 20.0f, -8.0f }, { 60.0f, -3.0f }, { 100.0f, 0.0f }, { 5000.0f, 0.0f },
        { 8000.0f, -6.0f }, { 12000.0f, -20.0f }, { 20000.0f, -30.0f } };
    static const std::vector<BreakPoint> lateTv = {
        { 20.0f, -7.0f }, { 50.0f, -3.0f }, { 100.0f, 0.0f }, { 10000.0f, 0.0f },
        { 14000.0f, -9.0f }, { 15500.0f, -30.0f }, { 20000.0f, -40.0f } };

    switch (mode)
    {
        case CurveMode::neutral:        return neutral;
        case CurveMode::academy:        return academy;
        case CurveMode::xCurve:         return xcurve;
        case CurveMode::xCurveSmallRoom:return xcurveSmall;
        case CurveMode::earlyTv:        return earlyTv;
        case CurveMode::kinescope:      return kinescope;
        case CurveMode::broadcastMono:  return broadcastMono;
        case CurveMode::lateTv:         return lateTv;
    }
    return neutral;
}

// Test targets: the bold anchor rows of DSP_SPEC §2.1.
inline const std::vector<TargetPoint>& testTargetsFor (CurveMode mode)
{
    static const std::vector<TargetPoint> neutral = {
        { 100.0f, 0.0f, 0.25f }, { 1000.0f, 0.0f, 0.25f }, { 8000.0f, 0.0f, 0.25f } };
    static const std::vector<TargetPoint> academy = {
        { 40.0f, -7.0f, 1.5f }, { 100.0f, 0.0f, 1.5f }, { 400.0f, 0.0f, 1.5f },
        { 1000.0f, 0.0f, 1.5f }, { 1600.0f, 0.0f, 1.5f }, { 5000.0f, -10.0f, 1.5f },
        { 8000.0f, -18.0f, 1.5f }, { 31.5f, -9.0f, 2.5f }, { 2500.0f, -2.5f, 2.5f } };
    static const std::vector<TargetPoint> xcurve = {
        { 200.0f, 0.0f, 1.0f }, { 1000.0f, 0.0f, 1.0f }, { 4000.0f, -3.0f, 1.0f },
        { 8000.0f, -6.0f, 1.0f }, { 16000.0f, -9.0f, 1.5f } };
    static const std::vector<TargetPoint> xcurveSmall = {
        { 1000.0f, 0.0f, 1.0f }, { 4000.0f, -1.5f, 1.0f }, { 8000.0f, -3.0f, 1.0f } };
    static const std::vector<TargetPoint> earlyTv = {
        { 80.0f, -6.0f, 2.5f }, { 400.0f, 0.0f, 1.5f }, { 6000.0f, -6.0f, 2.5f },
        { 10000.0f, -18.0f, 3.0f } };
    static const std::vector<TargetPoint> kinescope = {
        { 400.0f, 0.0f, 1.5f }, { 6000.0f, -10.0f, 3.0f }, { 9000.0f, -24.0f, 4.0f } };
    static const std::vector<TargetPoint> broadcastMono = {
        { 60.0f, -3.0f, 2.0f }, { 1000.0f, 0.0f, 1.0f }, { 8000.0f, -6.0f, 2.5f },
        { 12000.0f, -20.0f, 4.0f } };
    static const std::vector<TargetPoint> lateTv = {
        { 50.0f, -3.0f, 2.0f }, { 1000.0f, 0.0f, 1.0f }, { 5000.0f, 0.0f, 1.5f },
        { 14000.0f, -9.0f, 3.0f } };

    switch (mode)
    {
        case CurveMode::neutral:        return neutral;
        case CurveMode::academy:        return academy;
        case CurveMode::xCurve:         return xcurve;
        case CurveMode::xCurveSmallRoom:return xcurveSmall;
        case CurveMode::earlyTv:        return earlyTv;
        case CurveMode::kinescope:      return kinescope;
        case CurveMode::broadcastMono:  return broadcastMono;
        case CurveMode::lateTv:         return lateTv;
    }
    return neutral;
}

} // namespace vfa::dsp::curves
