#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <juce_dsp/juce_dsp.h>
#include "CurveTables.h"
#include "../Core/Filters.h"

// Minimum-phase FIR design from tabulated magnitude targets (ADR-004).
// Pipeline: log-frequency dB interpolation onto an FFT grid -> real cepstrum
// -> minimum-phase reconstruction -> windowed truncation.
//
// ALLOCATES — design-time only. Must never be called from the audio thread.

namespace vfa::dsp
{

class MinPhaseFirDesigner
{
public:
    // Interpolate breakpoints (dB over log f) onto a magnitude grid of
    // gridSize bins spanning [0, sampleRate/2].
    static std::vector<float> magnitudeGrid (const std::vector<curves::BreakPoint>& points,
                                             int gridSize, double sampleRate)
    {
        std::vector<float> mag ((size_t) gridSize);
        const double nyquist = sampleRate * 0.5;
        for (int i = 0; i < gridSize; ++i)
        {
            const double hz = nyquist * double (i) / double (gridSize - 1);
            mag[(size_t) i] = dbToGain (interpolateDb (points, float (std::max (hz, 1.0))));
        }
        return mag;
    }

    static float interpolateDb (const std::vector<curves::BreakPoint>& points, float hz)
    {
        if (points.empty()) return 0.0f;
        if (hz <= points.front().hz) return points.front().dB;
        if (hz >= points.back().hz)  return points.back().dB;
        for (size_t i = 1; i < points.size(); ++i)
        {
            if (hz <= points[i].hz)
            {
                const auto& p0 = points[i - 1];
                const auto& p1 = points[i];
                const float t = (std::log (hz) - std::log (p0.hz))
                              / (std::log (p1.hz) - std::log (p0.hz));
                return p0.dB + t * (p1.dB - p0.dB);
            }
        }
        return points.back().dB;
    }

    // Design a minimum-phase FIR of numTaps from a half-spectrum magnitude
    // grid (size fftSize/2 + 1). fftSize must be a power of two >= 4*numTaps.
    static std::vector<float> design (const std::vector<float>& halfMagnitude,
                                      int numTaps, int fftSize)
    {
        const int order = (int) std::log2 ((double) fftSize);
        juce::dsp::FFT fft (order);
        const int half = fftSize / 2;

        // Full complex spectrum of log-magnitude (floor to keep log finite).
        std::vector<std::complex<float>> spec ((size_t) fftSize);
        for (int i = 0; i <= half; ++i)
        {
            const float m = std::max (halfMagnitude[(size_t) std::min (i, (int) halfMagnitude.size() - 1)],
                                      1.0e-4f); // -80 dB floor
            spec[(size_t) i] = { std::log (m), 0.0f };
        }
        for (int i = 1; i < half; ++i)
            spec[(size_t) (fftSize - i)] = spec[(size_t) i];

        // Real cepstrum.
        std::vector<std::complex<float>> ceps ((size_t) fftSize);
        fft.perform (spec.data(), ceps.data(), true); // inverse

        // Fold: keep c[0], double positive quefrencies, zero negative ones.
        for (int i = 1; i < half; ++i)
        {
            ceps[(size_t) i] *= 2.0f;
            ceps[(size_t) (fftSize - i)] = { 0.0f, 0.0f };
        }
        // ceps[half] stays as-is.

        // Back to spectrum, exponentiate -> minimum-phase spectrum.
        std::vector<std::complex<float>> minSpec ((size_t) fftSize);
        fft.perform (ceps.data(), minSpec.data(), false);
        for (auto& c : minSpec)
            c = std::exp (c);

        // Impulse response.
        std::vector<std::complex<float>> impulse ((size_t) fftSize);
        fft.perform (minSpec.data(), impulse.data(), true);

        // Truncate with a raised-cosine tail over the last quarter.
        std::vector<float> taps ((size_t) numTaps);
        const int fadeStart = numTaps - numTaps / 4;
        for (int i = 0; i < numTaps; ++i)
        {
            float w = 1.0f;
            if (i >= fadeStart)
            {
                const float t = float (i - fadeStart) / float (numTaps - fadeStart);
                w = 0.5f * (1.0f + std::cos (kPi * t));
            }
            taps[(size_t) i] = impulse[(size_t) i].real() * w;
        }
        return taps;
    }

    static std::vector<float> designForCurve (const std::vector<curves::BreakPoint>& points,
                                              int numTaps, double sampleRate,
                                              int fftSize = 8192)
    {
        auto grid = magnitudeGrid (points, fftSize / 2 + 1, sampleRate);
        return design (grid, numTaps, fftSize);
    }
};

} // namespace vfa::dsp
