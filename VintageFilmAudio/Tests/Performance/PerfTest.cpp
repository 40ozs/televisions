#include "../Harness/VfaTest.h"
#include <chrono>
#include "DSP/Engine.h"

// Performance measurement (TEST_PLAN §Performance): full-chain cost across
// sample rates and block sizes, written to TestOutput/perf.json, with a
// regression guard on the Standard-quality 48 kHz / 512 case.

namespace
{
struct Measurement
{
    double sampleRate;
    int blockSize;
    double avgUs, worstUs, budgetUs, pctOfBudget;
};

Measurement measure (double sampleRate, int blockSize, vfa::dsp::Quality q)
{
    using namespace vfa::dsp;
    Engine engine;
    engine.prepare ({ sampleRate, blockSize, 2 });
    ParamSnapshot snap;                   // defaults: magneticFilm / academy
    snap.quality = q;
    engine.setSeed (42, 1);
    engine.designDeliveryNow (snap);
    engine.primeActive (snap);

    juce::AudioBuffer<float> buf (2, blockSize);
    Rng sig (7);

    auto fill = [&]
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            for (int i = 0; i < blockSize; ++i)
                d[i] = sig.nextBipolar() * 0.126f;
        }
    };

    // Warm-up.
    for (int b = 0; b < 30; ++b) { fill(); engine.process (buf, snap); }

    const int iterations = std::max (60, (int) (sampleRate / blockSize / 4)); // ~250 ms of audio
    double totalUs = 0, worstUs = 0;
    for (int b = 0; b < iterations; ++b)
    {
        fill();
        const auto t0 = std::chrono::steady_clock::now();
        engine.process (buf, snap);
        const auto t1 = std::chrono::steady_clock::now();
        const double us = std::chrono::duration<double, std::micro> (t1 - t0).count();
        totalUs += us;
        worstUs = std::max (worstUs, us);
    }
    const double avgUs = totalUs / iterations;
    const double budgetUs = 1.0e6 * blockSize / sampleRate;
    return { sampleRate, blockSize, avgUs, worstUs, budgetUs, 100.0 * avgUs / budgetUs };
}
} // namespace

VFA_TEST (Perf_full_chain_matrix)
{
    std::ofstream json (vfatest::testOutputDir() / "perf.json");
    json << "{\n  \"machine\": \"4-core container (see Docs/BENCHMARKS.md)\",\n  \"quality\": {\n";

    bool firstQ = true;
    double standard48k512Pct = -1.0;
    for (auto q : { vfa::dsp::Quality::eco, vfa::dsp::Quality::standard, vfa::dsp::Quality::high })
    {
        const char* qName = q == vfa::dsp::Quality::eco ? "eco"
                          : q == vfa::dsp::Quality::standard ? "standard" : "high";
        json << (firstQ ? "" : ",\n") << "    \"" << qName << "\": [\n";
        firstQ = false;

        bool first = true;
        for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
            for (int bs : { 16, 32, 64, 128, 256, 512, 1024, 2048 })
            {
                // Full matrix only at standard; corners elsewhere to bound runtime.
                if (q != vfa::dsp::Quality::standard
                    && ! ((sr == 48000.0 && (bs == 64 || bs == 512)) || (sr == 96000.0 && bs == 512)))
                    continue;
                const auto m = measure (sr, bs, q);
                json << (first ? "" : ",\n")
                     << "      {\"sr\": " << (int) m.sampleRate << ", \"block\": " << m.blockSize
                     << ", \"avg_us\": " << m.avgUs << ", \"worst_us\": " << m.worstUs
                     << ", \"budget_us\": " << m.budgetUs << ", \"pct\": " << m.pctOfBudget << "}";
                first = false;
                CHECK_MSG (m.pctOfBudget < 100.0, "over real-time budget");
                if (q == vfa::dsp::Quality::standard && sr == 48000.0 && bs == 512)
                    standard48k512Pct = m.pctOfBudget;
            }
        json << "\n    ]";
    }
    json << "\n  }\n}\n";

    char msg[96];
    std::snprintf (msg, sizeof (msg), "standard 48k/512 uses %.1f%% of budget", standard48k512Pct);
    CHECK_MSG (standard48k512Pct > 0 && standard48k512Pct < 50.0, msg);
}
