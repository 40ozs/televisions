#pragma once

#include <atomic>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Core/StreamSpec.h"
#include "../Core/ParamSnapshot.h"
#include "../Core/Filters.h"
#include "CurveTables.h"

namespace vfa::dsp
{

// Delivery/playback-curve stage (ADR-004, amended DEV-005): hybrid design.
//  - LF (< ~250 Hz): two RBJ low-shelf biquads auto-fitted at design time to
//    the curve table's LF target (short min-phase FIRs cannot realize LF
//    shelves; the fit is a bounded grid search, error test-enforced).
//  - HF (>= 250 Hz): minimum-phase FIR from the tabulated magnitude targets.
//  - Advanced LF Roll-Off and small Playback Size: analytic runtime
//    high-pass biquads (exact at any sample rate, no redesign needed).
// Design runs off the audio thread (or in prepare/designNow while audio is
// stopped); the audio thread crossfades current -> staged over ~10 ms.
// No allocation in process().
class DeliveryCurveModule
{
public:
    void prepare (const StreamSpec& spec);
    void reset();

    // Synchronous design; only while audio is not running (prepare time).
    void designNow (const ParamSnapshot& snap);

    // Message-thread redesign request. Returns false if a previous staging
    // is still pending (caller keeps its dirty flag and retries later).
    bool requestRedesign (const ParamSnapshot& snap);

    bool isStagingPending() const noexcept { return pending.load (std::memory_order_acquire) != 0; }

    void process (juce::AudioBuffer<float>& buffer, const ParamSnapshot& snap);

    // Full effective magnitude target (curve + advanced controls), for the
    // visualizer and documentation exports. Linear gains, [0..sr/2] grid.
    static std::vector<float> effectiveMagnitudeGrid (const ParamSnapshot& snap,
                                                      int gridSize, double sampleRate);

    int activeTapCount() const noexcept { return path[(size_t) currentIndex].len; }

    static constexpr float lfCrossoverHz = 250.0f;

private:
    struct LfFit { float fc1 = 100, g1 = 0, fc2 = 40, g2 = 0; };

    struct PathState
    {
        std::vector<float> taps;                  // maxTaps, first len used
        int len = 0;
        std::vector<std::vector<float>> history;  // per channel, 2*maxTaps
        std::vector<int> writePos;                // per channel
        std::vector<Biquad> shelf1, shelf2;       // per channel LF fit
        LfFit fit;
    };

    void designInto (PathState& state, const ParamSnapshot& snap);
    static int tapCountFor (Quality q, double sampleRate) noexcept;
    static LfFit fitLowShelves (const std::vector<curves::BreakPoint>& points,
                                float amount, double sampleRate);
    static double biquadMagnitudeDb (const Biquad& b, double hz, double sampleRate);

    float processSamplePath (PathState& s, int ch, float x) noexcept;

    PathState& current() noexcept { return path[(size_t) currentIndex]; }
    PathState& next() noexcept { return path[(size_t) (1 - currentIndex)]; }

    StreamSpec streamSpec;
    PathState path[2];
    int currentIndex = 0;
    std::atomic<int> pending { 0 };
    bool fading = false;
    float fade = 0.0f, fadeStep = 0.01f;

    // Runtime analytic filters (outside the crossfaded pair).
    std::vector<Biquad> lfRollHp;      // per channel, 2nd-order HP
    std::vector<Biquad> sizeHp;        // per channel, small-room LF loss
    float lastLfRollHz = -1.0f, lastSizeHpHz = -1.0f;

    static constexpr int maxTaps = 513;
};

} // namespace vfa::dsp
