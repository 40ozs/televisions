#include "../Harness/VfaTest.h"
#include <atomic>
#include <thread>
#include <cstdlib>
#include <new>
#include "Plugin/PluginProcessor.h"
#include "DSP/Core/Rng.h"

// Audio-thread allocation guard (PRD PR-12, TEST_PLAN §Realtime).
// Global new/delete are overridden for the whole test binary; allocations are
// counted only while the calling thread has auditing enabled, so a separate
// automation thread may allocate freely (as real hosts do).

namespace rtaudit
{
std::atomic<long long>& count()
{
    static std::atomic<long long> c { 0 };
    return c;
}
thread_local bool auditing = false;
} // namespace rtaudit

void* operator new (std::size_t size)
{
    if (rtaudit::auditing) rtaudit::count().fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (size > 0 ? size : 1)) return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t size)
{
    if (rtaudit::auditing) rtaudit::count().fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (size > 0 ? size : 1)) return p;
    throw std::bad_alloc();
}
void* operator new (std::size_t size, std::align_val_t al)
{
    if (rtaudit::auditing) rtaudit::count().fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::aligned_alloc ((size_t) al, ((size + (size_t) al - 1) / (size_t) al) * (size_t) al))
        return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t size, std::align_val_t al)
{
    return operator new (size, al);
}
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }
void operator delete (void* p, std::align_val_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::align_val_t) noexcept { std::free (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept { std::free (p); }

VFA_TEST (Realtime_no_allocation_in_processBlock)
{
    vfa::VfaProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    // Automation storm from another thread (its allocations are not audited).
    std::atomic<bool> stop { false };
    std::thread automation ([&proc, &stop]
    {
        vfa::dsp::Rng rng (99);
        const auto& metas = vfa::params::allParams();
        int presetIdx = 0;
        while (! stop.load())
        {
            for (int k = 0; k < 6; ++k)
            {
                const auto& meta = metas[(size_t) (rng.nextU64() % metas.size())];
                if (auto* p = proc.parameters().getParameter (meta.id))
                    p->setValueNotifyingHost (rng.next01());
            }
            if ((rng.nextU64() & 7) == 0)
                proc.setCurrentProgram (presetIdx++ % proc.getNumPrograms());
            std::this_thread::sleep_for (std::chrono::microseconds (200));
        }
    });

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    vfa::dsp::Rng sig (5);

    // Warm-up (first blocks may trigger lazily-initialized library state that
    // prepareToPlay legitimately owns).
    for (int b = 0; b < 20; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            for (int i = 0; i < 512; ++i) d[i] = sig.nextBipolar() * 0.1f;
        }
        proc.processBlock (buf, midi);
    }

    const long long before = rtaudit::count().load();
    long long audited = 0;
    for (int b = 0; b < 2000; ++b)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            for (int i = 0; i < 512; ++i) d[i] = sig.nextBipolar() * 0.1f;
        }
        rtaudit::auditing = true;
        proc.processBlock (buf, midi);
        rtaudit::auditing = false;
        ++audited;
    }
    const long long allocs = rtaudit::count().load() - before;

    stop.store (true);
    automation.join();

    char msg[128];
    std::snprintf (msg, sizeof (msg),
                   "%lld allocation(s) inside processBlock over %lld blocks",
                   allocs, audited);
    CHECK_MSG (allocs == 0, msg);
    CHECK (! vfatest::hasNanOrInf (buf.getReadPointer (0), 512));
}
