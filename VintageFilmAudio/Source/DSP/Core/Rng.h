#pragma once

#include <cstdint>

// Deterministic randomness (ADR-016): xoshiro256** with SplitMix64 seeding.
// Integer-only state transitions -> bit-exact reproducibility for a given
// seed on every platform. Never use std::rand/random_device/juce::Random in
// DSP code; derive independent streams with Rng::stream().

namespace vfa::dsp
{

class Rng
{
public:
    Rng() noexcept { seed (1); }
    explicit Rng (uint64_t s) noexcept { seed (s); }

    void seed (uint64_t s) noexcept
    {
        // SplitMix64 expansion of the seed into 256 bits of state.
        for (auto& v : state)
        {
            s += 0x9E3779B97F4A7C15ULL;
            uint64_t z = s;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            v = z ^ (z >> 31);
        }
    }

    // Derive an independent stream for (module, channel, purpose).
    static uint64_t streamSeed (uint64_t userSeed, uint32_t moduleId,
                                uint32_t channel, uint32_t purpose) noexcept
    {
        uint64_t h = userSeed ^ 0x9E3779B97F4A7C15ULL;
        auto mix = [&h] (uint64_t v) noexcept
        {
            h ^= v + 0x9E3779B97F4A7C15ULL + (h << 6) + (h >> 2);
            h *= 0xFF51AFD7ED558CCDULL;
            h ^= h >> 33;
        };
        mix (moduleId); mix (channel); mix (purpose);
        return h == 0 ? 1 : h;
    }

    uint64_t nextU64() noexcept
    {
        const uint64_t result = rotl (state[1] * 5, 7) * 9;
        const uint64_t t = state[1] << 17;
        state[2] ^= state[0];
        state[3] ^= state[1];
        state[1] ^= state[2];
        state[0] ^= state[3];
        state[2] ^= t;
        state[3] = rotl (state[3], 45);
        return result;
    }

    // Uniform in [0, 1). Fixed 2^-24 mapping for float determinism.
    float next01() noexcept
    {
        return float (nextU64() >> 40) * (1.0f / 16777216.0f);
    }

    // Uniform in [-1, 1).
    float nextBipolar() noexcept { return next01() * 2.0f - 1.0f; }

    // Approximately normal (Irwin–Hall, 4 uniforms), sigma ~= 1, bounded
    // to +-3.46. Avoids libm transcendentals for cross-platform determinism.
    float nextGauss() noexcept
    {
        return (next01() + next01() + next01() + next01() - 2.0f) * 1.7320508f;
    }

private:
    static constexpr uint64_t rotl (uint64_t x, int k) noexcept
    {
        return (x << k) | (x >> (64 - k));
    }

    uint64_t state[4] {};
};

} // namespace vfa::dsp
